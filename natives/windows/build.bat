@echo off
echo Building library for: x86; x86_64&echo:

cargo build --release --target=i686-pc-windows-msvc
cargo build --release --target=x86_64-pc-windows-msvc

echo: &echo:Finished!
pause