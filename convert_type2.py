import argparse
import csv
import datetime
import pandas as pd

from sklearn.metrics import fbeta_score, classification_report, confusion_matrix

vprofiles = [   {'Name': "P720p60fps16x9", 'Bitrate': "6000k", 'Framerate': 60, 'AspectRatio': "16:9", 'Resolution': "1280x720"},
                {'Name': "P720p30fps16x9", 'Bitrate': "4000k", 'Framerate': 30, 'AspectRatio': "16:9", 'Resolution': "1280x720"},
                {'Name': "P720p25fps16x9", 'Bitrate': "3500k", 'Framerate': 25, 'AspectRatio': "16:9", 'Resolution': "1280x720"},
                {'Name': "P720p30fps4x3", 'Bitrate': "3500k", 'Framerate': 30, 'AspectRatio': "4:3", 'Resolution': "960x720"},
                {'Name': "P576p30fps16x9", 'Bitrate': "1500k", 'Framerate': 30, 'AspectRatio': "16:9", 'Resolution': "1024x576"},
                {'Name': "P576p25fps16x9", 'Bitrate': "1500k", 'Framerate': 25, 'AspectRatio': "16:9", 'Resolution': "1024x576"},
                {'Name': "P360p30fps16x9", 'Bitrate': "1200k", 'Framerate': 30, 'AspectRatio': "16:9", 'Resolution': "640x360"},
                {'Name': "P360p25fps16x9", 'Bitrate': "1000k", 'Framerate': 25, 'AspectRatio': "16:9", 'Resolution': "640x360"},
                {'Name': "P360p30fps4x3", 'Bitrate': "1000k", 'Framerate': 30, 'AspectRatio': "4:3", 'Resolution': "480x360"},
                {'Name': "P240p30fps16x9", 'Bitrate': "600k", 'Framerate': 30, 'AspectRatio': "16:9", 'Resolution': "426x240"},
                {'Name': "P240p25fps16x9", 'Bitrate': "600k", 'Framerate': 25, 'AspectRatio': "16:9", 'Resolution': "426x240"},
                {'Name': "P240p30fps4x3", 'Bitrate': "600k", 'Framerate': 30, 'AspectRatio': "4:3", 'Resolution': "320x240"},
                {'Name': "P144p30fps16x9", 'Bitrate': "400k", 'Framerate': 30, 'AspectRatio': "16:9", 'Resolution': "256x144"},
                {'Name': "P144p25fps16x9", 'Bitrate': "400k", 'Framerate': 25, 'AspectRatio': "16:9", 'Resolution': "256x144"}
            ]

def getfields(row):
    id = -1
    res = {}
    profname = row['profile']
    for i in range(len(vprofiles)):
        if vprofiles[i]['Name'] == profname:
            id = i
            break

    if id != -1:
        tmp = vprofiles[id]['Name']
        res['attack'] = tmp[1:5] + "/" + "abcdefg.mp4" # abbreviation
        resolution = vprofiles[id]['Resolution'].split("x")
        res['dimension_x'] = int(resolution[0])
        res['dimension_y'] = int(resolution[1])

        res['fps'] = vprofiles[id]['Framerate']
        res['path'] = row['outpath']
        pixelnum = res['dimension_x'] * res['dimension_y']
        res['pixels'] = res['dimension_x'] * res['dimension_y'] * int(row['framecount'])

        mixdata = row['features']
        mixdata = mixdata.replace('"', '')
        datas = mixdata.split(',')
        res['size_dimension_ratio'] = float(datas[0])
        res['temporal_dct-mean'] = float(datas[1]) / pixelnum
        res['temporal_gaussian_mse-mean'] = float(datas[2]) / pixelnum
        res['temporal_gaussian_difference-mean'] = float(datas[3]) / pixelnum
        res['temporal_threshold_gaussian_difference-mean'] = float(datas[4])
        res['temporal_histogram_distance-mean'] = float(datas[5]) / pixelnum

        res['size'] = res['size_dimension_ratio'] * res['dimension_x'] * res['dimension_y']

    return  res

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--infile", default="d:/difffeature_lxnv.csv", help="csv file to calculate confusion matrix.")
    parser.add_argument("--outfile", default="out.csv", help="csv file to calculate confusion matrix.")
    args = parser.parse_args()
    infile = args.infile
    outfile = args.outfile

    label = []
    ypred = []

    FEATURES_FULL = ['id','title','attack','dimension_x','dimension_y','fps','path','pixels','size',
                   'size_dimension_ratio',
                   'temporal_dct-mean',
                   'temporal_gaussian_mse-mean',
                   'temporal_gaussian_difference-mean',
                   'temporal_threshold_gaussian_difference-mean',
                   'temporal_histogram_distance-mean'
                   ]

    #outcsv = "train_feed" + datetime.datetime.now().strftime("%m%d%H%M") + ".csv"

    wlist = []
    with open(infile, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            # label.append(0)
            wrow = {}
            wrow['id'] = row['outpath']
            wrow['title'] = row['filepath']

            tmp = getfields(row)
            wrow.update(tmp)

            ##wrow = row[FEATURES_FULL]
            wlist.append(wrow)

    df = pd.DataFrame(wlist)

    wdf = df[FEATURES_FULL]
    wdf.to_csv(outfile)

