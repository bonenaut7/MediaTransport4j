#include <iostream>
#include <vector>
#include <fstream>
#include <winrt/windows.foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include "uwp_impl.h"

using namespace std::chrono;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

// Configuration is here!!!
#define MT_SMTC_AWAIT_TIMEOUT 3s
#define MT_THUMBNAIL_AWAIT_TIMEOUT 1s

#ifdef DEBUG
#define MT_DEBUG(out) std::cout << out << std::endl;
#define MT_DEBUG_ERR(out) std::cerr << out << std::endl;
#else
#define MT_DEBUG(out) 
#define MT_DEBUG_ERR(out)
#endif

// This macro is here because I don't want to copy it's body 5 times with a very few changes, sorry ;C
#define SMTC_SESSION_INDEX_FUNCTION(sessionActionExpr) \
   try { \
        MT_DEBUG("[Action] SMTC Session action ->"); \
        const auto smtc = WaitForOrCancel(GlobalSystemMediaTransportControlsSessionManager::RequestAsync(), MT_SMTC_AWAIT_TIMEOUT); \
        if (smtc == nullptr) { \
            MT_DEBUG_ERR(" > Session action failed. SMTC Timeout."); \
            return JNI_FALSE; \
        } \
        const auto sessions = smtc.GetSessions(); \
        const int32_t index = static_cast<int32_t>(jObjectIndex); \
        const int32_t size = static_cast<int32_t>(sessions.Size()); \
        if (size != 0 && index > -1 && index < sessions.Size()) { \
            MT_DEBUG(" - [" << index << "] Session action passed."); \
            const auto session = sessions.GetAt(static_cast<uint32_t>(index)); \
            return sessionActionExpr ? JNI_TRUE : JNI_FALSE; \
        } \
    } catch (const hresult_error &err) { \
        MT_DEBUG_ERR(" > Session action failed. Exception: " << to_string(err.message())); \
    } \
    MT_DEBUG_ERR(" > Session action failed. Index out of bounds?"); \
    return JNI_FALSE;
// <- SMTC_SESSION_INDEX_FUNCTION

template <typename T>
static T WaitForOrCancel(IAsyncOperation<T> &op, const TimeSpan & timeout) {
    const AsyncStatus status = op.wait_for(timeout);
    if (status == AsyncStatus::Completed) {
        return op.get();
    }

    if (status == AsyncStatus::Started) {
        op.Cancel();
    }

    return nullptr;
}

// Warning: Spaghetti-code!
JNIEXPORT jobject JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaTransport_nParseSessions(JNIEnv *env, jclass obj) {
    // Looks like we can't put these references into global fields... well, what a shame.
    MT_DEBUG("[nParseSessions] Searching for JNI classes and methods...");
    const jclass jArrayListClass = env->FindClass("Ljava/util/ArrayList;");
    const jclass jByteBufferClass = env->FindClass("Ljava/nio/ByteBuffer;");
    const jclass jWindowsMediaSectionClass = env->FindClass("Lby/bonenaut7/mediatransport4j/impl/windows/WindowsMediaSession;");
    const jmethodID jArrayListConstructor = env->GetMethodID(jArrayListClass, "<init>", "()V");
    const jmethodID jWindowsMediaSectionConstructor = env->GetMethodID(jWindowsMediaSectionClass, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/nio/ByteBuffer;JJZ)V");
    const jmethodID jArrayList_Add = env->GetMethodID(jArrayListClass, "add", "(Ljava/lang/Object;)Z");
    const jmethodID jByteBuffer_Wrap = env->GetStaticMethodID(jByteBufferClass, "wrap", "([B)Ljava/nio/ByteBuffer;");

    // Spaghetti-code!
    jobject arrayList = nullptr;
    IRandomAccessStreamWithContentType asyncStream = nullptr;
    DataReader reader = nullptr;
    bool streamsClosed = false;

    try {
        MT_DEBUG("[nParseSessions] Acquiring SMTC...");
        const auto smtc = WaitForOrCancel(GlobalSystemMediaTransportControlsSessionManager::RequestAsync(), MT_SMTC_AWAIT_TIMEOUT);
        if (smtc == nullptr) {
            MT_DEBUG_ERR("[nParseSessions]  > SMTC acquire timeout!");
            return NULL;
        }

        MT_DEBUG("[nParseSessions] Create JNI list object");
        arrayList = env->NewObject(jArrayListClass, jArrayListConstructor);

        MT_DEBUG("[nParseSessions] Getting SMTC sessions...");
        auto smtcSessions = smtc.GetSessions();
        for (int idx = 0; idx < smtcSessions.Size(); idx++) {
            const auto smtcSession = smtcSessions.GetAt(idx);
            const auto sessionMediaProps = smtcSession.TryGetMediaPropertiesAsync().get();
            const auto sessionTimeline = smtcSession.GetTimelineProperties();
            const auto sessionThumbnail = sessionMediaProps.Thumbnail();

            // Acquire titles, playback information
            MT_DEBUG("[nParseSessions][" << idx << "] Acquiring session information...");
            jstring sourceApp = env->NewStringUTF(to_string(smtcSession.SourceAppUserModelId()).c_str());
            jstring artist = env->NewStringUTF(to_string(sessionMediaProps.Artist()).c_str());
            jstring title = env->NewStringUTF(to_string(sessionMediaProps.Title()).c_str());
            jboolean isPlaying = smtcSession.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            jlong duration = duration_cast<seconds>(sessionTimeline.EndTime() - sessionTimeline.StartTime()).count();
            jlong position = duration_cast<seconds>(
                isPlaying ? clock::now() - sessionTimeline.LastUpdatedTime() + sessionTimeline.Position() :
                sessionTimeline.Position()
            ).count();

            // Gather thumbnail byte array
            MT_DEBUG("[nParseSessions][" << idx << "] Acquiring thumbnail...");
            jbyteArray thumbnailArray;
            if (sessionThumbnail) {
                MT_DEBUG("[nParseSessions][" << idx << "] Getting thumbnail into a byte array");
                asyncStream = WaitForOrCancel(sessionThumbnail.OpenReadAsync(), MT_THUMBNAIL_AWAIT_TIMEOUT);
                if (asyncStream != nullptr) {
                    reader = DataReader(asyncStream);

                    MT_DEBUG("[nParseSessions][" << idx << "] Getting thumbnail stream size");
                    reader.LoadAsync(asyncStream.Size()).get();

                    // Length-check
                    if (asyncStream.Size() < (std::numeric_limits<uint32_t>::max() >> 1)) { // 2's complement shift
                        MT_DEBUG("[nParseSessions][" << idx << "] Read thumbnail size");
                        std::vector<uint8_t> buffer(asyncStream.Size());
                        const auto bufferArrayView = array_view(buffer);
                        reader.ReadBytes(bufferArrayView);

                        const jsize bufferSize = static_cast<jsize>(buffer.size());
                        thumbnailArray = env->NewByteArray(bufferSize);
                        env->SetByteArrayRegion(thumbnailArray, 0, bufferSize, reinterpret_cast<const jbyte*>(buffer.data()));
                    }
                    else { // I mean Java have 4-byte size limit(int) of the byte arrays length :b
                        MT_DEBUG_ERR("[nParseSessions][" << idx << "]  > Thumbnail buffer size out of int32_t bounds");
                        thumbnailArray = env->NewByteArray(0);
                    }

                    reader.Close();
                    asyncStream.Close();
                    streamsClosed = true; // Yes, there's a catch, the first one can be closed, but the second one could fail
                } else {
                    MT_DEBUG("[nParseSessions][" << idx << "]  > Thumbnail async-stream timeout");
                    thumbnailArray = env->NewByteArray(0);
                }
                
            } else {
                MT_DEBUG("[nParseSessions][" << idx << "] Making empty thumbnail byte array");
                thumbnailArray = env->NewByteArray(0);
            }

            // Create ByteBuffer for the thumbnail
            MT_DEBUG("[nParseSessions][" << idx << "] Create thumbnail ByteBuffer");
            jobject thumbnailBuffer = env->CallStaticObjectMethod(jByteBufferClass, jByteBuffer_Wrap, thumbnailArray);
            env->DeleteLocalRef(thumbnailArray);

            // Create WindowsMediaSession
            MT_DEBUG("[nParseSessions][" << idx << "] Create WindowsMediaSession");
            jobject sessionObject = env->NewObject(jWindowsMediaSectionClass, jWindowsMediaSectionConstructor,
                idx, sourceApp, artist, title, thumbnailBuffer, duration, position, isPlaying
            );

            // Clean references
            env->DeleteLocalRef(sourceApp);
            env->DeleteLocalRef(artist);
            env->DeleteLocalRef(title);
            env->DeleteLocalRef(thumbnailBuffer);

            // Put WindowsMediaSession to the array list
            MT_DEBUG("[nParseSessions][" << idx << "] Put WindowsMediaSession into the ArrayList");
            env->CallBooleanMethod(arrayList, jArrayList_Add, sessionObject);
            env->DeleteLocalRef(sessionObject);
        }

        MT_DEBUG("[nParseSessions] Return list");
        return arrayList;
    } catch (const hresult_error& err) {
        MT_DEBUG_ERR("[nParseSessions] Exception ocurred: " << to_string(err.message()));

        // Destroy array list cause we will return NULL anyways
        if (arrayList != nullptr) {
            env->DeleteLocalRef(arrayList);
            arrayList = nullptr;
        }

        // Make sure to close all streams
        // I barely know how to do it properly in cxx...
        if (!streamsClosed) {
            if (asyncStream != nullptr) {
                asyncStream.Close();
            }
            if (reader != nullptr) {
                reader.Close();
            }
        }
    }
    
    return NULL;
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nSwitchToNext(JNIEnv *env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TrySkipNextAsync());
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nSwitchToPrevious(JNIEnv* env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TrySkipPreviousAsync());
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nPlay(JNIEnv* env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TryPlayAsync());
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nPause(JNIEnv* env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TryPauseAsync());
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nTogglePlay(JNIEnv* env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TryTogglePlayPauseAsync());
}

JNIEXPORT jboolean JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaSession_nStop(JNIEnv* env, jclass obj, jint jObjectIndex) {
    SMTC_SESSION_INDEX_FUNCTION(session.TryStopAsync());
}