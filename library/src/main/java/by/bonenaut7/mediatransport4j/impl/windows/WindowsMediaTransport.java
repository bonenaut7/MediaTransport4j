package by.bonenaut7.mediatransport4j.impl.windows;

import java.util.List;

import by.bonenaut7.mediatransport4j.api.MediaSession;
import by.bonenaut7.mediatransport4j.api.MediaTransportInterface;

public final class WindowsMediaTransport implements MediaTransportInterface {

	@Override
	public List<MediaSession> parseSessions() {
		return nParseSessions();
	}
	
	private static native List<MediaSession> nParseSessions();
}
