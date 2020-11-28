import cv2
import argparse
import datetime
import csv
import glob
import numpy as np
import pandas as pd

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", default="testdataset", help="directory of video dataset.")
    args = parser.parse_args()
    testdir = args.dir

    fileset = [file for file in glob.glob(testdir + "**/*", recursive=True)]
    count = len(fileset) - 1
    binit = False

    outcsv = "testlist" + datetime.datetime.now().strftime("%Y%m%d%H%M%S") + ".csv"
    fileout = open(outcsv, 'w', newline='')
    wr = csv.writer(fileout)
    wr.writerow(['filepath', 'width', 'height', 'fps', 'bitrate', 'framecount', 'indices'])
    max_samples = 10

    for file in fileset:
        fname = os.path.basename(file)
        print(fname + ": ")

        try:
            cap = cv2.VideoCapture(file, cv2.CAP_FFMPEG)
            frelpath = file.replace(testdir + "\\", "")
            width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = cap.get(cv2.CAP_PROP_FPS)
            bitrate = int(cap.get(cv2.CAP_PROP_BITRATE))
            frame_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
            #generate randomize 10 indices

            indexes = np.sort(np.random.choice(frame_count, max_samples, False))
            strindices = '"' + str(indexes[0])
            for i in range(1, max_samples):
                strindices += ("," + str(indexes[i]))
            strindices += '"'

            #write dataset list
            wr.writerow([frelpath, width, height, fps, bitrate, frame_count, strindices])

        except Exception as e:
            print(e)
        #finally:
            #if cap is not None:
            #    cap.release()

    print('Success!')
    fileout.close()
