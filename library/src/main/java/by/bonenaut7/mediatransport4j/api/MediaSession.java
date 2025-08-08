package by.bonenaut7.mediatransport4j.api;

import java.nio.ByteBuffer;

//TODO: Add albums information
//String getAlbumArtist();
//String getAlbumTitle();
public interface MediaSession {
	
	/**
	 * Returns application name that "owns" information about the media (i.e. the
	 * media player application name).
	 * 
	 * @return source application name
	 */
	String getSourceApp();
	
	/**
	 * Switches media playback to the another media, next one in the queue, if there is any.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been occured.
	 */
	boolean switchToNext();
	
	/**
	 * Switches media playback to the another media, previous one in the queue, if there is any.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been occured.
	 */
	boolean switchToPrevious();
	
	/**
	 * Returns media artist, for example, this may return the "Unknown artist" from
	 * the "Unknown artist - Example song".
	 * 
	 * @return media artist
	 */
	String getArtist();
	
	/**
	 * Returns media title, for example, this may return the "Example song" from the
	 * "Unknown artist - Example song".
	 * 
	 * @return media title
	 */
	String getTitle();
	
	/**
	 * Returns whether {@link #getThumbnail()} has image data, or it's empty.
	 * 
	 * @return <code>true</code> if {@link #getThumbnail()} has image data, <code>false</code> otherwise.
	 */
	boolean hasThumbnail();
	
	/**
	 * Gets the ByteBuffer object with the thumbnail (i.e. image information,
	 * pixels).
	 * 
	 * @return buffer with image data
	 */
	ByteBuffer getThumbnail();
	
	/**
	 * Gets the media playback duration in seconds.
	 * 
	 * @return media duration in seconds
	 */
	long getDuration();
	
	/**
	 * Gets the media playback position in seconds.
	 * 
	 * @return media position in seconds
	 */
	long getPosition();
	
	/**
	 * Gets whether media is playing, or it's stopped or paused.
	 * 
	 * @return <code>true</code> if media is playing, <code>false</code> otherwise.
	 */
	boolean isPlaying();
	
	/**
	 * Starts or continue playing of the media.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been occured.
	 */
	boolean play();
	
	/**
	 * Pauses playing of the media.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been occured.
	 */
	boolean pause();
	
	/**
	 * Starts or continue playing of the media if the media is paused or stopped, or
	 * pauses the media if it's playing.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been
	 *         occured.
	 */
	boolean togglePlay();

	/**
	 * Stops playing of the media.
	 * 
	 * @return <code>true</code> if succeed, <code>false</code> if error has been occured.
	 */
	boolean stop();
}
