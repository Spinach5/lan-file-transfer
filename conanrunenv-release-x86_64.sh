script_folder="/home/zqw/biancheng/Project/websocket"
echo "echo Restoring environment" > "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
for v in LD_LIBRARY_PATH DYLD_LIBRARY_PATH OPENSSL_MODULES PATH
do
   is_defined="true"
   value=$(printenv $v) || is_defined="" || true
   if [ -n "$value" ] || [ -n "$is_defined" ]
   then
       echo export "$v='$value'" >> "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
   else
       echo unset $v >> "$script_folder/deactivate_conanrunenv-release-x86_64.sh"
   fi
done

export LD_LIBRARY_PATH="/home/zqw/.conan2/p/b/libwe7e21572057626/p/lib:/home/zqw/.conan2/p/b/opensde2aceaa2abcb/p/lib:/home/zqw/.conan2/p/b/libarca7ca13f0788c/p/lib:/home/zqw/.conan2/p/b/zlibc0a23da583554/p/lib:/home/zqw/.conan2/p/b/libicb0b56ae16d90a/p/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DYLD_LIBRARY_PATH="/home/zqw/.conan2/p/b/libwe7e21572057626/p/lib:/home/zqw/.conan2/p/b/opensde2aceaa2abcb/p/lib:/home/zqw/.conan2/p/b/libarca7ca13f0788c/p/lib:/home/zqw/.conan2/p/b/zlibc0a23da583554/p/lib:/home/zqw/.conan2/p/b/libicb0b56ae16d90a/p/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
export OPENSSL_MODULES="/home/zqw/.conan2/p/b/opensde2aceaa2abcb/p/lib/ossl-modules"
export PATH="/home/zqw/.conan2/p/b/opensde2aceaa2abcb/p/bin:/home/zqw/.conan2/p/b/libicb0b56ae16d90a/p/bin${PATH:+:$PATH}"