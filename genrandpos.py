import os
import cv2
import argparse
import datetime
import csv
import glob
import numpy as np
import random

import pandas as pd

def getstring(digitarray):

    max_samples = len(digitarray)
    strarray = '"' + str(digitarray[0])
    for i in range(1, max_samples):
        strarray += ("," + str(digitarray[i]))
    strarray += '"'
    return strarray


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", default="", help="video file directory")
    parser.add_argument("--infile", default="list.csv", help="template video file list.")
    args = parser.parse_args()

    infile = args.infile
    dir = args.dir

    outcsv = "randlist" + datetime.datetime.now().strftime("%m%d%H") + ".csv"
    fileout = open(outcsv, 'w', newline='')
    wr = csv.writer(fileout)
    wr.writerow(['filepath', 'width', 'height', 'fps', 'bitrate',
                 'profile', 'devmode', 'framecount', 'indices', 'position', 'length'])
    max_samples = 10

    brheader = False
    with open(infile) as csvfile:
        rd = csv.reader(csvfile, delimiter=',')
        for row in rd:
            print(row)
            wrow = []
            if brheader == False:
                brheader = True
            else:
                vfile = dir + "/" + row[0]
                file_stats = os.stat(vfile)

                positions = np.sort(np.random.choice(file_stats.st_size, max_samples, False))
                lenghts = np.random.choice(5000000, max_samples, False)

                spositions = getstring(positions)
                slenghts = getstring(lenghts)

                wrow = row
                wrow.append(spositions)
                wrow.append(slenghts)
                wr.writerow(wrow)

    fileout.close()
    print('Success generate random positions!')