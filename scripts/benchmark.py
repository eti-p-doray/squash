import argparse
import logging
import os
import subprocess
import sys
import tempfile

import utility


def main(argv):
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(description="""
    Runs benchmarks
    """, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('outdir',
                        help='Working directory')
    parser.add_argument('-i', '--indir', default = 'testdata',
                        help='Directory containing test data')
    parser.add_argument('-t', '--tooldir', default='src/out/Release',
                        help='Directory containing binaries')
    args = parser.parse_args()

    indir = args.indir
    tooldir = args.tooldir
    outdir = os.path.join('out', args.outdir)
    if outdir is None:
        outdir = tempfile.mkdtemp('out')
    
    print(utility.make_filename('win64', 526414))

    logging.info('input directory: %s', indir)
    logging.info('tool directory: %s', tooldir)
    logging.info('output directory: %s', outdir)

    revisions = [445272, 454726, 464841, 475179, 488823, 499356, 508891, 526414]


    dataset = [
      {'old':445272, 'new':454726, 'file':'chrome.dll'},
      {'old':445272, 'new':454726, 'file':'chrome_child.dll'},
      {'old':454726, 'new':464841, 'file':'chrome.dll'},
      {'old':454726, 'new':464841, 'file':'chrome_child.dll'},
      {'old':464841, 'new':475179, 'file':'chrome.dll'},
      {'old':464841, 'new':475179, 'file':'chrome_child.dll'},
      {'old':475179, 'new':488823, 'file':'chrome.dll'},
      {'old':475179, 'new':488823, 'file':'chrome_child.dll'},
      {'old':488823, 'new':499356, 'file':'chrome.dll'},
      {'old':488823, 'new':499356, 'file':'chrome_child.dll'},
      {'old':499356, 'new':508891, 'file':'chrome.dll'},
      {'old':499356, 'new':508891, 'file':'chrome_child.dll'},
      {'old':508891, 'new':526414, 'file':'chrome.dll'},
      {'old':508891, 'new':526414, 'file':'chrome_child.dll'}
    ]

    zucchini = os.path.join(tooldir, "zucchini")
    for case in dataset:
      old_file =  os.path.join(utility.make_filename('win64', case['old']), case['file'])
      new_file = os.path.join(utility.make_filename('win64', case['new']), case['file'])
      patch_file = '%s_%s_%s_%s.zuc' % ('win64', case['old'], case['new'], case['file'])
      compressed_patch_file = '%s_%s_%s_%s.zuc.7z' % ('win64', case['old'], case['new'], case['file'])
      
      old_path = os.path.join(indir, old_file)
      new_path = os.path.join(indir, new_file)
      patch_path = os.path.join(outdir, patch_file)
      compressed_patch_path = os.path.join(outdir, compressed_patch_file)

      if not os.path.isfile(compressed_patch_path):
        if not os.path.isfile(patch_path):
          utility.generate_patch(zucchini, old_path, new_path, patch_path)
        utility.compress(patch_path, compressed_patch_path)

      logging.info('file size: %d', os.path.getsize(compressed_patch_path))

if __name__ == "__main__":
    main(sys.argv[1:])
