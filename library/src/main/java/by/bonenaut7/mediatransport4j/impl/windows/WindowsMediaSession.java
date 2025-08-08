package by.bonenaut7.mediatransport4j.impl.windows;

import java.nio.ByteBuffer;

import by.bonenaut7.mediatransport4j.impl.AbstractMediaSession;

public final class WindowsMediaSession extends AbstractMediaSession {
	private int sessionIndex = -1;
	
	private WindowsMediaSession(
		int sessionIndex,
		String sourceApp,
		String artist,
		String title,
		ByteBuffer thumbnail,
		long duration,
		long position,
		boolean isPlaying
	) {
		this.sessionIndex = sessionIndex;
		this.sourceApp = sourceApp;
		this.artist = artist;
		this.title = title;
		this.thumbnail = thumbnail;
		this.duration = duration;
		this.position = position;
		this.isPlaying = isPlaying;
	}
	
	@Override
	public boolean switchToNext() {
		return nSwitchToNext(sessionIndex);
	}

	@Override
	public boolean switchToPrevious() {
		return nSwitchToPrevious(sessionIndex);
	}

	@Override
	public boolean play() {
		return nPlay(sessionIndex);
	}

	@Override
	public boolean pause() {
		return nPause(sessionIndex);
	}

	@Override
	public boolean togglePlay() {
		return nTogglePlay(sessionIndex);
	}

	@Override
	public boolean stop() {
		return nStop(sessionIndex);
	}
	
	private static native boolean nSwitchToNext(int index);
	
	private static native boolean nSwitchToPrevious(int index);
	
	private static native boolean nPlay(int index);
	
	private static native boolean nPause(int index);
	
	// TODO Make it return 0b11 mask: 0b1 (fail: 0; success: 1); 0b01 (isPaused: 0; isPlaying: 1)
	private static native boolean nTogglePlay(int index);
	
	private static native boolean nStop(int index);
}
