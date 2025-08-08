package by.bonenaut7.mediatransport4j.impl;

import java.nio.ByteBuffer;

import by.bonenaut7.mediatransport4j.api.MediaSession;

public abstract class AbstractMediaSession implements MediaSession {
	protected String sourceApp;
	protected String artist;
	protected String title;
	protected ByteBuffer thumbnail;
	protected long duration;
	protected long position;
	protected boolean isPlaying;
	
	@Override
	public String getSourceApp() {
		return this.sourceApp;
	}
	
	@Override
	public String getArtist() {
		return this.artist;
	}

	@Override
	public String getTitle() {
		return this.title;
	}
	
	@Override
	public boolean hasThumbnail() {
		return this.thumbnail.capacity() != 0;
	}

	@Override
	public ByteBuffer getThumbnail() {
		return this.thumbnail;
	}

	@Override
	public long getDuration() {
		return this.duration;
	}

	@Override
	public long getPosition() {
		return this.position;
	}

	@Override
	public boolean isPlaying() {
		return this.isPlaying;
	}
}
