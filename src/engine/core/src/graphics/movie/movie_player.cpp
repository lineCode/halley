#include "halley/core/graphics/movie/movie_player.h"
#include "resources/resources.h"
#include "halley/concurrency/concurrent.h"
#include "halley/core/graphics/texture_descriptor.h"
#include "halley/core/graphics/texture.h"
#include "halley/core/graphics/render_target/render_target_texture.h"
#include "halley/core/api/video_api.h"
#include "halley/core/graphics/material/material_definition.h"
#include "halley/core/graphics/render_context.h"
#include "halley/audio/audio_clip.h"
#include <chrono>
#include "halley/audio/audio_position.h"

using namespace Halley;
using namespace std::chrono_literals;

MoviePlayer::MoviePlayer(VideoAPI& video, AudioAPI& audio)
	: video(video)
	, audio(audio)
	, threadRunning(false)
{
}

MoviePlayer::~MoviePlayer()
{
	stopThread();
}

void MoviePlayer::play()
{
	if (state == MoviePlayerState::Paused) {
		startThread();

		if (!streamingClip) {
			streamingClip = std::make_shared<StreamingAudioClip>(2);
		}

		renderTexture = video.createTexture(getSize());
		auto desc = TextureDescriptor(getSize(), TextureFormat::RGBA);
		desc.useFiltering = true;
		desc.isRenderTarget = true;
		renderTexture->load(std::move(desc));

		renderTarget = video.createTextureRenderTarget();
		renderTarget->setTarget(0, renderTexture);
		renderTarget->setViewPort(Rect4i(Vector2i(), getSize()));

		state = MoviePlayerState::StartingToPlay;

		for (auto& s: streams) {
			s.playing = true;
		}
	}
}

void MoviePlayer::pause()
{
	if (state == MoviePlayerState::Playing) {
		state = MoviePlayerState::Paused;

		for (auto& s: streams) {
			s.playing = false;
		}

		audioHandle->stop();
	}
}

void MoviePlayer::reset()
{
	stopThread();

	state = MoviePlayerState::Paused;
	time = 0;

	currentTexture.reset();
	pendingFrames.clear();
	streamingClip.reset();
	if (audioHandle) {
		audioHandle->stop();
		audioHandle.reset();
	}

	onReset();
}

void MoviePlayer::update(Time t)
{
	if (state == MoviePlayerState::Playing) {
		time += t;
	}

	if (state == MoviePlayerState::Playing || state == MoviePlayerState::StartingToPlay) {
		if (!pendingFrames.empty()) {
			auto& next = pendingFrames.front();
			if (time >= next.time) {
				currentTexture = next.texture;
				pendingFrames.pop_front();
			}
		}

		if (state == MoviePlayerState::Playing && pendingFrames.empty() && !needsMoreVideoFrames()) {
			state = MoviePlayerState::Finished;
		}

		if (state == MoviePlayerState::StartingToPlay) {
			if (!needsMoreVideoFrames() && !needsMoreAudioFrames()) {
				if (streamingClip) {
					audioHandle = audio.play(streamingClip, AudioPosition::makeUI(), 0.5f);
				}
				state = MoviePlayerState::Playing;
			}
		}
	}

}

void MoviePlayer::render(Resources& resources, RenderContext& rc)
{
	if (currentTexture) {
		Camera cam;
		cam.setPosition(Vector2f(videoSize) * 0.5f);
		cam.setZoom(1.0f);

		auto c = rc.with(*renderTarget).with(cam);
		c.bind([&] (Painter& painter)
		{
			auto matDef = resources.get<MaterialDefinition>("Halley/NV12Video");
			Sprite().setImage(currentTexture, matDef).setTexRect(Rect4f(0, 0, 1, 1)).setSize(Vector2f(videoSize)).draw(painter);
		});
		currentTexture.reset();
	}
}

Sprite MoviePlayer::getSprite(Resources& resources)
{
	if (renderTexture) {
		auto matDef = resources.get<MaterialDefinition>("Halley/Sprite");
		return Sprite().setImage(renderTexture, matDef).setTexRect(Rect4f(0, 0, 1, 1)).setSize(Vector2f(videoSize));
	} else {
		return Sprite().setMaterial(resources, "Halley/SolidColour").setColour(Colour4f(0, 0, 0)).setSize(Vector2f(videoSize));
	}
}

MoviePlayerState MoviePlayer::getState() const
{
	return state;
}

Vector2i MoviePlayer::getSize() const
{
	return videoSize;
}

void MoviePlayer::startThread()
{
	if (!threadRunning) {
		threadRunning = true;
		workerThread = std::thread([=] () {
			threadEntry();
		});
	}
}

void MoviePlayer::stopThread()
{
	if (threadRunning) {
		threadRunning = false;
		workerThread.join();
	}
}

void MoviePlayer::threadEntry()
{
	while (threadRunning) {
		bool hadWork = false;
		if (needsMoreAudioFrames()) {
			requestAudioFrame();
			hadWork = true;
		} 
		if (needsMoreVideoFrames()) {
			requestVideoFrame();
			hadWork = true;
		}
		
		if (!hadWork) {
			std::this_thread::sleep_for(2ms);
		}
	}
}

bool MoviePlayer::needsMoreVideoFrames() const
{
	std::unique_lock<std::mutex> lock(mutex);

	if (state != MoviePlayerState::Playing && state != MoviePlayerState::StartingToPlay) {
		return false;
	}

	const MoviePlayerStream* videoStream = nullptr;
	for (auto& s: streams) {
		if (s.type == MoviePlayerStreamType::Video) {
			videoStream = &s;
			break;
		}
	}

	if (!videoStream) {
		return false;
	}

	return pendingFrames.size() < 7 && !videoStream->eof;
}

bool MoviePlayer::needsMoreAudioFrames() const
{
	std::unique_lock<std::mutex> lock(mutex);

	if (state != MoviePlayerState::Playing && state != MoviePlayerState::StartingToPlay) {
		return false;
	}

	const MoviePlayerStream* audioStream = nullptr;
	for (auto& s: streams) {
		if (s.type == MoviePlayerStreamType::Audio) {
			audioStream = &s;
		}
	}

	if (!audioStream) {
		return false;
	}

	if (!streamingClip) {
		return false;
	}

	return streamingClip->getSamplesLeft() < 20000 && !audioStream->eof;
}

void MoviePlayer::setVideoSize(Vector2i size)
{
	videoSize = size;
}

VideoAPI& MoviePlayer::getVideoAPI() const
{
	return video;
}

AudioAPI& MoviePlayer::getAudioAPI() const
{
	return audio;
}

void MoviePlayer::onVideoFrameAvailable(Time time, TextureDescriptor&& descriptor)
{
	auto desc = std::make_shared<TextureDescriptor>(std::move(descriptor));

	Concurrent::execute(Executors::getVideoAux(), [this, time, desc(std::move(desc))] ()
	{
		auto descriptor = TextureDescriptor(std::move(*desc));
		std::shared_ptr<Texture> tex;

		{
			std::unique_lock<std::mutex> lock(mutex);

			if (recycleTexture.empty()) {
				tex = video.createTexture(descriptor.size);
			} else {
				tex = recycleTexture.front();
				recycleTexture.pop_front();
			}
			tex->startLoading();
			pendingFrames.push_back({tex, time});
		}

		tex->load(std::move(descriptor));
	});
}

void MoviePlayer::onAudioFrameAvailable(Time time, gsl::span<const AudioConfig::SampleFormat> samples)
{
	streamingClip->addInterleavedSamples(samples);	
}
