# This script is invoked to uninstall the hcfft library and test sources
# Preliminary version

# CURRENT_WORK_DIRECTORY
current_work_dir=$PWD

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
install=0

# Help menu
print_help() {
cat <<-HELP
=============================================================================================================================
Please provide the following arguments:

  ${green}--deepclean${reset}  To clean the files under /opt/rocm/hcfft  Requires sudo perms.

=============================================================================================================================
Usage: ./clean.sh --deepclean 
=============================================================================================================================
HELP
exit 0
}

while [ $# -gt 0 ]; do
  case "$1" in
    --deepclean)
      deepclean="1"
      ;;
    --help) print_help;;
    *)
      printf "************************************************************\n"
      printf "* Error: Invalid arguments, run --help for valid arguments.*\n"
      printf "************************************************************\n"
      exit 1
  esac
  shift
done

# Remove build
rm -rf $current_work_dir/build

# Remove files under /opt/rocm/hcrng
if [ "$deepclean" = "1" ]; then
  sudo rm -rf /opt/rocm/hcfft
  sudo rm -f /opt/rocm/lib/libhcfft.so
  sudo rm -f /opt/rocm/lib/libhipfft.so
fi

# Remove temporarly cached kernel shared objects and binaries
rm -rf $HOME/kernCache

# TODO: ADD More options


