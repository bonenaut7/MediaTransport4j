package by.bonenaut7.mediatransport4j.api;

import java.util.List;

import by.bonenaut7.mediatransport4j.SharedLibraryLoader;
import by.bonenaut7.mediatransport4j.impl.windows.WindowsMediaTransport;

public final class MediaTransport {
	private static final SharedLibraryLoader LIBRARY_LOADER = new SharedLibraryLoader();
	private static MediaTransport instance;
	private final MediaTransportInterface transportInterface;
	
	private MediaTransport() {
		if (SharedLibraryLoader.isWindows) {
			LIBRARY_LOADER.load("mediatransport4j");
			transportInterface = new WindowsMediaTransport();
		} else {
			transportInterface = null;
		}
	}
	
	/**
	 * Library and natives initialization, loads native libraries and sets up
	 * everything. Must be called prior to {@link MediaTransport#getMediaSessions()
	 * getMediaSessions()}.
	 * 
	 * @return <code>true</code> if library has initialized, <code>false</code> if
	 *         something went wrong.
	 */
	public static final boolean init() {
		if (instance == null) {
			instance = new MediaTransport();
			
			return instance.transportInterface != null;
		}
		
		return false;
	}
	
	/**
	 * Gets a list of active media sessions
	 * 
	 * @return list of active media sessions, or <code>null</code> if media sessions
	 *         information is not available at the moment.
	 */
	public static final List<MediaSession> getMediaSessions() {
		return instance.transportInterface.parseSessions();
	}
}
