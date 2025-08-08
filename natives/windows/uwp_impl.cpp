#include <iostream>
#include <vector>
#include <fstream>
#include <winrt/windows.foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include "uwp_impl.h"

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

#ifdef DEBUG
#define MT_DEBUG(out) std::cout << out << std::endl;
#else
#define MT_DEBUG(out) 
#endif

#define SMTC_ACTION(codeblock, onException) \
    try { \
        codeblock \
    } catch (const hresult_error& err) { \
        return NULL; \
    }

// This macro is here because I don't want to copy it's body 5 times with a very few changes, sorry ;C
#define SMTC_SESSION_INDEX_FUNCTION(sessionActionExpr) \
    SMTC_ACTION( \
        MT_DEBUG("SMTC Action ->"); \
        const auto smtc = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get(); \
        const auto sessions = smtc.GetSessions(); \
        const int32_t index = static_cast<int32_t>(jObjectIndex); \
        const int32_t size = static_cast<int32_t>(sessions.Size()); \
        if (size != 0 && index > -1 && index < sessions.Size()) { \
            MT_DEBUG(" - Session " << idx); \
            const auto session = sessions.GetAt(static_cast<uint32_t>(index)); \
            return sessionActionExpr ? JNI_TRUE : JNI_FALSE; \
        } \
        MT_DEBUG(" - Session not found"); \
        return JNI_FALSE; \
   ) \

// Gross globals!
static bool g_globalsSaved = false;
static jclass g_jArrayListClass = nullptr;
static jclass g_jByteBufferClass = nullptr;
static jclass g_jWindowsMediaSectionClass = nullptr;
static jmethodID g_jArrayListConstructor = nullptr;
static jmethodID g_jWindowsMediaSectionConstructor = nullptr;
static jmethodID g_jArrayList_Add = nullptr;
static jmethodID g_jByteBuffer_Wrap = nullptr;

JNIEXPORT jobject JNICALL Java_by_bonenaut7_mediatransport4j_impl_windows_WindowsMediaTransport_nParseSessions(JNIEnv *env, jclass obj) {
    // This may be moved to it's own function along with SMTC per-process instance? Maybe...
    if (!g_globalsSaved) {
        MT_DEBUG("Creating globals");
        g_jArrayListClass = env->FindClass("Ljava/util/ArrayList;");
        g_jByteBufferClass = env->FindClass("Ljava/nio/ByteBuffer;");
        g_jWindowsMediaSectionClass = env->FindClass("Lby/bonenaut7/mediatransport4j/impl/windows/WindowsMediaSession;");
        g_jArrayListConstructor = env->GetMethodID(g_jArrayListClass, "<init>", "()V");
        g_jWindowsMediaSectionConstructor = env->GetMethodID(g_jWindowsMediaSectionClass, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/nio/ByteBuffer;JJZ)V");
        g_jArrayList_Add = env->GetMethodID(g_jArrayListClass, "add", "(Ljava/lang/Object;)Z");
        g_jByteBuffer_Wrap = env->GetStaticMethodID(g_jByteBufferClass, "wrap", "([B)Ljava/nio/ByteBuffer;");
    }

    jobject arrayList = nullptr;
    IRandomAccessStreamWithContentType asyncStream = nullptr;
    DataReader reader = nullptr;
    bool streamsClosed = false;

    try {
        MT_DEBUG("Acquire SMTC");
        const auto smtc = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

        MT_DEBUG("Create JNI ArrayList object");
        arrayList = env->NewObject(g_jArrayListClass, g_jArrayListConstructor);

        MT_DEBUG("Get SMTC sessions");
        auto smtcSessions = smtc.GetSessions();
        for (int idx = 0; idx < smtcSessions.Size(); idx++) {
            const auto smtcSession = smtcSessions.GetAt(idx);
            const auto sessionMediaProps = smtcSession.TryGetMediaPropertiesAsync().get();
            const auto sessionTimeline = smtcSession.GetTimelineProperties();
            const auto sessionThumbnail = sessionMediaProps.Thumbnail();

            // Acquire titles, playback information
            MT_DEBUG("[" << idx << "] Acquire information");
            jstring sourceApp = env->NewStringUTF(to_string(smtcSession.SourceAppUserModelId()).c_str());
            jstring artist = env->NewStringUTF(to_string(sessionMediaProps.Artist()).c_str());
            jstring title = env->NewStringUTF(to_string(sessionMediaProps.Title()).c_str());
            jboolean isPlaying = smtcSession.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            jlong duration = std::chrono::duration_cast<std::chrono::seconds>(sessionTimeline.EndTime() - sessionTimeline.StartTime()).count();
            jlong position = std::chrono::duration_cast<std::chrono::seconds>(
                isPlaying ? clock::now() - sessionTimeline.LastUpdatedTime() + sessionTimeline.Position() :
                sessionTimeline.Position()
            ).count();

            // Gather thumbnail byte array
            MT_DEBUG("[" << idx << "] Acquire thumbnail");
            jbyteArray thumbnailArray;
            if (sessionThumbnail != nullptr) {
                MT_DEBUG("[" << idx << "] Get thumbnail into a byte array");
                asyncStream = sessionThumbnail.OpenReadAsync().get();
                reader = DataReader(asyncStream);

                MT_DEBUG("[" << idx << "] Get thumbnail stream size");
                reader.LoadAsync(asyncStream.Size()).get();
               
                // Length-check
                if (asyncStream.Size() < (std::numeric_limits<uint32_t>::max() >> 1)) { // 2's complement shift
                    MT_DEBUG("[" << idx << "] Read thumbnail size");
                    std::vector<uint8_t> buffer(asyncStream.Size());
                    const auto bufferArrayView = array_view(buffer);
                    reader.ReadBytes(bufferArrayView);
                    
                    const jsize bufferSize = static_cast<jsize>(buffer.size());
                    thumbnailArray = env->NewByteArray(bufferSize);
                    env->SetByteArrayRegion(thumbnailArray, 0, bufferSize, reinterpret_cast<const jbyte*>(buffer.data()));
                } else { // I mean Java have 4-byte size limit(int) of the byte arrays length :b
                    MT_DEBUG("[" << idx << "] Thumbnail buffer size out of int32_t bounds");
                    thumbnailArray = env->NewByteArray(0);
                }

                reader.Close();
                asyncStream.Close();
                streamsClosed = true; // Yes, there's a catch, the first one can be closed, but the second one could fail
            } else {
                MT_DEBUG("[" << idx << "] Make empty thumbnail byte array");
                thumbnailArray = env->NewByteArray(0);
            }

            // Create ByteBuffer for the thumbnail
            MT_DEBUG("[" << idx << "] Create thumbnail ByteBuffer");
            jobject thumbnailBuffer = env->CallStaticObjectMethod(g_jByteBufferClass, g_jByteBuffer_Wrap, thumbnailArray);
            env->DeleteLocalRef(thumbnailArray);

            // Create WindowsMediaSession
            MT_DEBUG("[" << idx << "] Create WindowsMediaSession");
            jobject sessionObject = env->NewObject(g_jWindowsMediaSectionClass, g_jWindowsMediaSectionConstructor,
                idx, sourceApp, artist, title, thumbnailBuffer, duration, position, isPlaying
            );

            // Clean references
            env->DeleteLocalRef(sourceApp);
            env->DeleteLocalRef(artist);
            env->DeleteLocalRef(title);
            env->DeleteLocalRef(thumbnailBuffer);

            // Put WindowsMediaSession to the array list
            MT_DEBUG("[" << idx << "] Put WindowsMediaSession into the ArrayList");
            env->CallBooleanMethod(arrayList, g_jArrayList_Add, sessionObject);
            env->DeleteLocalRef(sessionObject);
        }

        MT_DEBUG("Return list");
        return arrayList;
    } catch (const hresult_error& err) {
        MT_DEBUG("Oh, no, an exception! Message: " << to_string(err.message()));

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