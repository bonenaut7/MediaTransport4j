# MediaTransport4j
Allows you to control OS media sessions

# How to use
Step 1. Initialize the library
```java
if (!MediaTransport.init()) {
    // Initialization may fail in some circumstances.
    // If you sure it's a bug, create an Issue and describe the issue.
    throw RuntimeException("Initialization failed");
}
```

Step 2. Query active media sessions
```java
List<MediaSession> sessions = MediaTransport.getMediaSessions(); // Returned list may be null!
```

Step 3. Get the information you need, or control media sessions
```java
if (sessions != null) {
    System.out.println("Active sessions:");
    for (MediaSession session : sessions) {
        System.out.printf("%s - %s\n", session.getAuthor(), session.getTitle());
    }
}
```

Well, that's all you need to know!

# How to build
### Java library
To build the java library, go to the "library" folder and run `gradle build`.  
Built java archives will be in the "library/build/libs/" folder.  

### Natives
To build the natives, go to the "natives/{platform}/" folder (for example "natives/windows/")  
Run `init.bat` or `init.sh` file to init **cmake** project and build the library.  
**Note for windows platform: windows natives are using UWP stuff to get information about media sessions, so anything older than Windows 10 will be probably unsupported and won't work at all.**

### Natives naming
**MediaTransport4j** uses [**jnigen-loader**](https://github.com/libgdx/gdx-jnigen) to load the native libraries. **jnigen-loader** uses it's own naming for the native libraries, so make sure to name them properly before putting them into the runtime jar file:
| OS Type | x86 | x64 | arm-x86 | arm-x64 |
| ------- | --- | --- | ------- | ------- |
| Windows | `mediatransport4j.dll` | `mediatransport4j64.dll` | `mediatransport4jarm.dll` | `mediatransport4jarm64.dll` |
| Linux   | `mediatransport4j.so` | `mediatransport4j64.so` | `mediatransport4jarm.so` | `mediatransport4jarm64.so` |
| MacOS   | `mediatransport4j.dylib` | `mediatransport4j64.dylib` | `mediatransport4jarm.dylib` | `mediatransport4jarm64.dylib` |
