import glob
import os.path
import argparse
import subprocess
import csv
import math
import platform
import os.path
import datetime
import time
import cv2
import extractfts
import numpy as np
from pathlib import Path

def rename_files(srcdir):

    for fpath in glob.iglob(srcdir + "/**/*.lib", recursive=True):
        strtmp = fpath
        strtmp = strtmp.replace('2413','')
        os.rename(fpath, strtmp)

def calc_norefpsnr(videopath):
    #rename_files("C:/workspace/windows/msys64/usr/local/lib")
    rename_files("E:/lib")

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

def calc_dctdiff(renditionpath , srcpath):
    dctval = {}
    #renditionpath = renditionpath.encode()
    #srcpath = srcpath.encode()

    if os.path.isfile(renditionpath) == False:
        dctval['doprocess'] = False
        return dctval
    if os.path.isfile(srcpath) == False:
        dctval['doprocess'] = False
        return dctval

    try:
        destdct = extractfts.loadft(renditionpath,0, 5, 32, 480, 270)
        srcdct = extractfts.loadft(srcpath, 0, 5, 32, 480, 270)

        if len(destdct) > 0 & len(srcdct):
            _, max_val, _, _ = cv2.minMaxLoc(srcdct - destdct)
            mean_val = np.mean((srcdct - destdct) ** 2)

            dctval['diffmax'] = max_val
            dctval['diffmse'] = mean_val
            dctval['doprocess'] = True

    except Exception as e:
        print(e)

    return dctval

#python calcdctdiff.py --infile="test_data.csv" --diroriginal="E:/TestData/livepeer-verifier-originals"
# --dirrendition="E:/TestData/livepeer-verifier-renditions"
def main():
    calc_norefpsnr("")
    calc_dctdiff("E:/TestData/livepeer-verifier-renditions/480p_watermark-345x114/eUCFCRCbKRw.mp4","E:/TestData/livepeer-verifier-renditions/480p_watermark-345x114/eUCFCRCbKRw.mp4")

    parser = argparse.ArgumentParser(description='dct benchmark parser.')
    #parser.add_argument('--dirtampertrue', type=str, help='select tamper video directory')
    #parser.add_argument('--dirtamperfalse', type=str, help='select normal transcoded video directory')
    parser.add_argument('--diroriginal', type=str, help='select original video directory')
    parser.add_argument('--dirrendition', type=str, help='select rendition video directory')
    parser.add_argument('--infile', type=str, help='select csv file')
    args = parser.parse_args()

    infile = args.infile
    diroriginal = args.diroriginal
    dirrendition = args.dirrendition

    inflag = False

    if infile != None:
        inflag = True
        if diroriginal == None:
            print("select video directory!")
            return

    filelist = []
    originallist = []
    targetlist = []

    #srcdirpath = os.path.commonpath(srcdir)

    if infile != None:
        with open(infile) as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                print(row['id'], row['target'])
                filelist.append( row['id'])
                originallist.append(row['source'])
                targetlist.append(row['target'])

        #for i in range(1, len(list_of_rows)):
        #filelist.append(list_of_rows[i][0])
    elif dirrendition != None:
        #filelist = [file for file in glob.glob(srcdir + "/**/*.mp4")]
        for fpath in glob.iglob(dirrendition + "/**/*.mp4", recursive=True):
            strtmp = fpath.replace(dirrendition + "\\",'')
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

    outcsv = "resultdct" + datetime.datetime.now().strftime("%Y%m%d%H%M%S") + ".csv"

    totalcount = len(filelist)
    fileout = open(outcsv, 'w', newline='')
    wr = csv.writer(fileout)

    wr.writerow([0, 'filepath', 'diffmax', 'diffmse', 'target'])

    start_time = time.time()

    successnum = 0
    for i in range(0, len(filelist)):
        print(filelist[i])
        try:
            vpath = filelist[i]
            orgpath = ""
            if inflag:
                vpath = dirrendition + "/" + filelist[i]
                orgpath = diroriginal + "/" + originallist[i]

            dctval = calc_dctdiff(vpath, orgpath)
            print(dctval)

            if dctval['doprocess'] == True:
                wr.writerow([successnum + 1, filelist[i], dctval['diffmax'], dctval['diffmse'], targetlist[i]])
                successnum = successnum + 1

        except Exception as e:
            print(e)

    totaltime = (time.time() - start_time)
    speed = totaltime / totalcount

    print(speed, 'per one file')

    fileout.close()

if __name__== "__main__":
    main()