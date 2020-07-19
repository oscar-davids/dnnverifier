import glob
import os.path
import argparse
import subprocess
import csv
import math
import platform
import os.path
import extractfts

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

    if infile != None:
        filelist = []
        with open(infile, 'r') as csvfile:
            # pass the file object to reader() to get the reader object
            csv_reader = csv.reader(csvfile)
            # Pass reader object to list() to get a list of lists
            list_of_rows = list(csv_reader)
        for i in range(1, len(list_of_rows)):
            filelist.append(list_of_rows[i][0])
    elif srcdir != None:
        filelist = [file for file in glob.glob(srcdir + "**/*.mp4")]
    else:
        print("empty argument")
        return

    #mepath = os.path.abspath(os.path.dirname(__file__))
    #vpath = os.path.join(mepath, srcdir)

    filepsnr = open('write.csv', 'w')
    wr = csv.writer(filepsnr)

    wr.writerow([0, 'filepath', 'psnr'])

    for i in range(0, len(filelist)):
        print(filelist[i])
        try:
            psnr = calc_norefpsnr(filelist[i])
        except Exception as e:
            print(e)

        print(psnr)
        wr.writerow([i + 1, filelist[i], psnr])


    filepsnr.close()

if __name__== "__main__":
    main()