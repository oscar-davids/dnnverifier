import glob
import os.path
import argparse
import subprocess
import csv
import math
import platform
import os.path
import extractfts
import datetime
import time

from pathlib import Path


def calc_norefpsnr(videopath):

    psnr = 0.0
    if os.path.isfile(videopath) == False:
        return psnr
    try:
        fbitrate = extractfts.get_bitrate(videopath)
        fqp1 = extractfts.get_qpi(videopath)

        # psnr = extractfts.get_psnr(videopath)
        psnr = 74.791 - 2.215 * math.log10(fbitrate) - 0.975 * fqp1 + 0.00001708 * fbitrate * fqp1
    except Exception as e:
        print(e)

    return psnr

def main():
    parser = argparse.ArgumentParser(description='psnr calculation parser.')
    parser.add_argument('--dir', type=str, help='select directory')
    parser.add_argument('--infile', type=str, help='select csv file')
    args = parser.parse_args()

    infile = args.infile
    srcdir = args.dir
    inflag = False

    if infile != None:
        inflag = True
        if srcdir == None:
            print("select video directory!")
            return

    filelist = []
    targetlist = []

    #srcdirpath = os.path.commonpath(srcdir)

    if infile != None:
        with open(infile) as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                print(row['id'], row['target'])
                filelist.append( row['id'])
                targetlist.append(row['target'])

        #for i in range(1, len(list_of_rows)):
        #filelist.append(list_of_rows[i][0])
    elif srcdir != None:
        #filelist = [file for file in glob.glob(srcdir + "/**/*.mp4")]
        for fpath in glob.iglob(srcdir + "/**/*.mp4", recursive=True):
            strtmp = fpath.replace(srcdir + "\\",'')
            if len(strtmp.split("\\")) and len(strtmp.split("\\")[0]) == 4:
                filelist.append(fpath)
                targetlist.append(0)
            else:
                filelist.append(fpath)
                targetlist.append(1)

        #filelist = [file for file in Path(srcdir).rglob("*.mp4")]
        #targetlist = [0 for _ in range(len(filelist))]
    else:
        print("empty argument")
        return

    #mepath = os.path.abspath(os.path.dirname(__file__))
    #vpath = os.path.join(mepath, srcdir)

    outcsv = "resultpsnr" + datetime.datetime.now().strftime("%Y%m%d%H%M%S") + ".csv"

    totalcount = len(filelist)
    filepsnr = open(outcsv, 'w', newline='')
    wr = csv.writer(filepsnr)

    wr.writerow([0, 'filepath', 'psnr', 'target'])

    start_time = time.time()

    for i in range(0, len(filelist)):
        print(filelist[i])
        try:
            vpath = filelist[i]
            if inflag:
                vpath = srcdir + "/" + filelist[i]
            psnr = calc_norefpsnr(vpath)

        except Exception as e:
            print(e)

        print(psnr)
        wr.writerow([i + 1, filelist[i], psnr, targetlist[0]])

    totaltime = (time.time() - start_time)
    speed = totaltime / totalcount

    print(speed, 'per one file')

    filepsnr.close()

if __name__== "__main__":
    main()
