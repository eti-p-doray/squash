import logging
import subprocess

def get_listing_platform_dir(platform):
    if platform == 'win':
      return 'Win'
    elif platform == 'win64':
      return 'Win_x64'

def make_filename(platform, revision):
  return "%s%%2F%s%%2Fchrome-win32" % (get_listing_platform_dir(platform), revision)

def generate_patch(tool, old_file, new_file, patch_file):
    command = [tool, '-gen', old_file, new_file, patch_file]
    logging.debug('running: %s', ' '.join(command))
    subprocess.check_call(command)

def apply_patch(tool, old_file, patch_file, new_file):
    command = [tool, '-apply', old_file, patch_file, new_file]
    logging.debug('running: %s', ' '.join(command))
    subprocess.check_call(command)

def compress(in_file, out_file):
    command = ['7z', 'a', '-t7z', '-m0=BCJ2', '-m1=LZMA:d27:fb128',
               '-m2=LZMA:d22:fb128:mf=bt2', '-m3=LZMA:d22:fb128:mf=bt2',
               '-mb0:1', '-mb0s1:2', '-mb0s2:3', out_file, in_file]
    logging.debug('running: %s', ' '.join(command))
    subprocess.check_call(command)
