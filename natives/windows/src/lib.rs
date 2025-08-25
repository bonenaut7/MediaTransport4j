#![allow(non_snake_case)]

use std::{
    string::ToString,
    sync::OnceLock,
    time::{ SystemTime, UNIX_EPOCH },
};
use tokio::{
    runtime::{ Runtime, Builder },
    time::{ timeout, Duration }
};
use jni::{
    JNIEnv,
    objects::{ JClass, JObject, JByteArray, JValue },
    sys::{ jobject, jboolean, jint, jlong, jbyte, jsize, JNI_TRUE, JNI_FALSE },
};
use windows_future::IAsyncOperation;
use windows::{
    core::{ Error, Result, HSTRING, RuntimeType },
    Foundation::TimeSpan,
    Media::Control::{
        GlobalSystemMediaTransportControlsSessionManager,
        GlobalSystemMediaTransportControlsSession,
        GlobalSystemMediaTransportControlsSessionPlaybackStatus
    },
    Storage::Streams::{ IRandomAccessStreamWithContentType, DataReader }
};

// Config
const MEDIA_TRANSPORT_TIMEOUT: Duration = Duration::from_millis(3000);
const THUMBNAIL_STREAM_TIMEOUT: Duration = Duration::from_millis(1000);

static RUNTIME: OnceLock<Runtime> = OnceLock::new(); // tokio weird runtime

macro_rules! unwrap_or_break {
    ($expr:expr, $label:tt) => {
        match $expr {
            Ok(val) => val,
            Err(_) => break $label,
        }
    };
}

macro_rules! unwrap_or_break_opt {
    ($expr:expr, $label:tt) => {
        match $expr {
            Some(val) => val,
            None => break $label,
        }
    };
}


#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaTransport_nParseSessions<'local>(mut env: JNIEnv<'local>, _: JClass<'local>) -> jobject {
    return parseSessions(&mut env).unwrap_or(0 as jobject);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nSwitchToNext<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint)  -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TrySkipNextAsync()).unwrap_or_else(|_| JNI_FALSE);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nSwitchToPrevious<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint) -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TrySkipPreviousAsync()).unwrap_or_else(|_| JNI_FALSE);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nPlay<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint) -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TryPlayAsync()).unwrap_or_else(|_| JNI_FALSE);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nPause<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint) -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TryPauseAsync()).unwrap_or_else(|_| JNI_FALSE);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nTogglePlay<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint) -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TryTogglePlayPauseAsync()).unwrap_or_else(|_| JNI_FALSE);
}

#[unsafe(no_mangle)]
pub extern "C" fn Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nStop<'local>(env: JNIEnv<'local>, obj: JClass<'local>, jObjectIndex: jint) -> jboolean {
    return doSMTCAction(jObjectIndex, |session| session.TryStopAsync()).unwrap_or_else(|_| JNI_FALSE);
}

struct JNIData<'local> {
    byteBufferClass: JClass<'local>,
    windowsMediaSectionClass: JClass<'local>
}

fn parseSessions(env: &mut JNIEnv) -> Option<jobject> {
    let arrayListClass: JClass = env.find_class("Ljava/util/ArrayList;").ok()?;
    let byteBufferClass: JClass = env.find_class("Ljava/nio/ByteBuffer;").ok()?;
    let windowsMediaSectionClass: JClass = env.find_class("Lby/bonenaut7/mediatransport4j/impl/windows/WindowsMediaSession;").ok()?;

    let data = JNIData { byteBufferClass, windowsMediaSectionClass };
    let sessions = getMediaTransport().ok()?.GetSessions().ok()?;
    let sessionsSize = sessions.Size().ok()?;
    let arrayList = env.new_object(&arrayListClass, "()V", &[ ]).ok()?;

    for idx in 0..sessionsSize {
        let _ = createSessionObject(env, idx, sessions.GetAt(idx), &data).map(|obj| {
            let object: JObject = unsafe { JObject::from_raw(obj) };
            let _ = env.call_method(&arrayList, "add", "(Ljava/lang/Object;)Z", &[ JValue::from(&object) ]);
            let _ = env.delete_local_ref(object);
        });
    }

    return Some(arrayList.into_raw());
}

fn createSessionObject<'local>(env: &mut JNIEnv<'local>, index: u32, sessionResult: Result<GlobalSystemMediaTransportControlsSession>, data: &JNIData) -> Result<jobject> {
    let session = sessionResult?;
    let mediaProps = session.TryGetMediaPropertiesAsync()?.get()?;
    let sessionTimeline = session.GetTimelineProperties()?;
    let sessionThumbnail = mediaProps.Thumbnail(); // nullable

    let sourceApp = env.new_string(convertString(session.SourceAppUserModelId())).unwrap();
    let artist = env.new_string(convertString(mediaProps.Artist())).unwrap();
    let title = env.new_string(convertString(mediaProps.Title())).unwrap(); // Don't care about unwrap since otherwise JVM would be dead anyways
    let isPlaying = session.GetPlaybackInfo()
        .ok()
        .and_then(|pi| pi.PlaybackStatus().ok())
        .map(|ps| ps == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
        .unwrap_or(false) as jboolean;
    let duration: jlong = (getTimeSpan(sessionTimeline.EndTime()) - getTimeSpan(sessionTimeline.StartTime())) as jlong;
    let position: jlong = if (isPlaying == JNI_TRUE) {
        let now = SystemTime::now().duration_since(UNIX_EPOCH).ok().map(|d| d.as_secs()).unwrap_or(0_u64);
        let lastUpdated = sessionTimeline.LastUpdatedTime().ok().map(|dt| dt.UniversalTime as u64).unwrap_or(now);
        (now - lastUpdated + getTimeSpan(sessionTimeline.Position())) as jlong // Unwrap(lastUpdated) or return the same amount(as `now`), so the diff will be zero
    } else {
        getTimeSpan(sessionTimeline.Position()) as jlong
    };

    let mut thumbnailArray: Option<JByteArray> = None;
    let mut streamOp: Option<IRandomAccessStreamWithContentType> = None;
    let mut readerOp: Option<DataReader> = None;
    if (sessionThumbnail.is_ok()) {
        let _ = 'thumb: {
            let thumbnail = sessionThumbnail.unwrap();
            let asyncStream = unwrap_or_break!(waitForOrCancel(unwrap_or_break!(thumbnail.OpenReadAsync(), 'thumb), THUMBNAIL_STREAM_TIMEOUT), 'thumb);
            streamOp = Some(asyncStream.clone()); // Caching stream (ГОВНО)

            let reader = unwrap_or_break!(DataReader::CreateDataReader(&asyncStream), 'thumb);
            readerOp = Some(reader.clone()); // Caching reader (ГОВНО)

            let asyncStreamSize = unwrap_or_break!(asyncStream.Size(), 'thumb) as u32;

            // Read the stream
            let _ = unwrap_or_break!(reader.LoadAsync(asyncStreamSize), 'thumb).get();

            // Copy to a JByteBuffer
            if (asyncStreamSize < (u32::MAX >> 1)) {
                thumbnailArray = env.new_byte_array(asyncStreamSize as jsize).ok();
                let _ = thumbnailArray.as_ref().map(|jb| {
                    let mut buffer = vec![0u8; asyncStreamSize as usize];
                    let _ = reader.ReadBytes(&mut buffer);

                    let jbuffer: Vec<jbyte> = buffer.iter().map(|&b| b as jbyte).collect();
                    let _ = env.set_byte_array_region(&jb, 0, &jbuffer);

                });
            } else {
                thumbnailArray = env.new_byte_array(0).ok(); // Thumbnail is too big?
            }
        };
    } else {
        thumbnailArray = env.new_byte_array(0).ok(); // There's no thumbnail available
    }

    // If array is here
    let mut byteBufferObj: Option<JObject<'local>> = None;
    thumbnailArray.map(|jb| {
        byteBufferObj = env.call_static_method(
            &data.byteBufferClass,
            "wrap",
            "([B)Ljava/nio/ByteBuffer;",
            &[ JValue::from(&jb) ]
        ).map(|jvo| jvo.l().ok()).unwrap_or(None);
        let _ = env.delete_local_ref(jb);
    });

    let mut sessionObject: Result<jobject> = Err(Error::empty());
    byteBufferObj.map(|b| {
        let _ = env.new_object(
            &data.windowsMediaSectionClass,
            "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/nio/ByteBuffer;JJZ)V",
            &[
                JValue::from(index as i32),
                JValue::from(&sourceApp),
                JValue::from(&artist),
                JValue::from(&title),
                JValue::from(&b),
                JValue::from(duration),
                JValue::from(position),
                JValue::from(isPlaying)
            ]
        ).map(|obj| sessionObject = Ok(obj.into_raw()));
        let _ = env.delete_local_ref(b);
    });

    // Closing resources
    readerOp.map(|s| s.Close());
    streamOp.map(|s| s.Close());

    // Deleting references
    let _ = env.delete_local_ref(sourceApp);
    let _ = env.delete_local_ref(artist);
    let _ = env.delete_local_ref(title);

    return sessionObject;
}

fn doSMTCAction(index: jint, fun: fn(GlobalSystemMediaTransportControlsSession) -> Result<IAsyncOperation<bool>>) -> Result<jboolean> {
    return Ok(fun(getSMTCSession(index as u32)?)?.get()? as jboolean);
}

fn getSMTCSession(index: u32) -> Result<GlobalSystemMediaTransportControlsSession> {
    let sessions = getMediaTransport()?.GetSessions()?;
    if (index < sessions.Size()?) {
        return sessions.GetAt(index);
    }

    return Err(Error::empty());
}

#[inline]
fn getMediaTransport() -> Result<GlobalSystemMediaTransportControlsSessionManager> {
    return waitForOrCancel(GlobalSystemMediaTransportControlsSessionManager::RequestAsync()?, Duration::from_secs(5));
}

fn waitForOrCancel<T: RuntimeType>(op: IAsyncOperation<T>, duration: Duration) -> Result<T> {
    let closeableFuture = (&op).clone();
    let rt: &Runtime = RUNTIME.get_or_init(|| Builder::new_current_thread().enable_all().build().unwrap());

    rt.block_on(async {
        return match timeout(duration, op.into_future()).await {
            Ok(Ok(object)) => Ok(object),
            _ => {
                let _ = closeableFuture.Cancel();
                return Err(Error::empty());
            }
        };
    })
}

#[inline]
fn convertString(result: Result<HSTRING>) -> String {
    return result.map_or_else(|_| String::from("Unknown"), |h| h.to_string() );
}

#[inline]
fn getTimeSpan(result: Result<TimeSpan>) -> u64 {
    return result.ok().map(|ts| ts.Duration as u64).unwrap_or(0_u64);
}