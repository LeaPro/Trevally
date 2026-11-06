if [[ -e $(realpath toolchainSetup) ]] ; then
  unset LD_LIBRARY_PATH
  source toolchainSetup
  if [[ -z $SDKTARGETSYSROOT ]] ; then
    echo "SDKTARGETSYSROOT is not set; something is amiss with your sdk environment :("
    exit 1
  fi
  make "$@"
else
  echo "toolchainSetup does not exist: it should be a symbolic link to your yocto sdk environment-setup script"
  echo "e.g.: ln -s ../../yocto-scarthgap/sdk/trevally/environment-setup-aarch64-oe-linux toolchainSetup"
  echo "(note that the location may vary by user, so the link is not kept in the git repo)"
  exit 1
fi
