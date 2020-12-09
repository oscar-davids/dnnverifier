import csv
import argparse
import numpy
from scipy.spatial import distance
import seaborn as sns
import matplotlib.pyplot as plt
import datetime

def getfieldcount(fname):
    with open(fname) as csv_file:
        lval = 0
        csv_reader = csv.reader(csv_file, delimiter=',')
        flag = True
        for row in csv_reader:
            lval = len(row)
            break
    return lval

def getvallist(fname):
    with open(fname) as csv_file:
        lval = []
        csv_reader = csv.reader(csv_file, delimiter=',')
        flag = True
        for row in csv_reader:
            if flag == True:
                flag = False
                continue
            lval.append(row)
    return lval

def covertdigitlist(slist, index):
    listdisit = []
    for row in slist:
        positions = row[index].replace('"',"")
        lpos = positions.split(',')
        disitpos = [float(i) for i in lpos]
        listdisit.append(disitpos)

    return listdisit

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--inf1", default="in1.csv", help="1st csv file for evaluation.")
    parser.add_argument("--inf2", default="in2.csv", help="2nd csv file for evaluation.")
    parser.add_argument("--target", type=int, default=None, help='target value for true or false')
    parser.add_argument("--ofile", default="", help="distance matrix for .")
    args = parser.parse_args()
    incsv1 = args.inf1
    incsv2 = args.inf2
    target = args.target
    ofile = args.ofile
    if target is not None:
        if ofile == "":
            ofile = "diffdis" + datetime.datetime.now().strftime("d%H%M") + ".csv"
    else:
        ofile = ""

    #check path
    fieldnum = getfieldcount(incsv1)
    listvall = getvallist(incsv1)
    listval2 = getvallist(incsv2)

    assert(len(listvall) == len(listval2))

    posidx = fieldnum - 2
    listpos1 = numpy.array(covertdigitlist(listvall,posidx))
    listlen1 = numpy.array(covertdigitlist(listvall,posidx+1))
    listpos2 = numpy.array(covertdigitlist(listval2,posidx))
    listlen2 = numpy.array(covertdigitlist(listval2,posidx+1))

    diffpos = listpos1 - listpos2
    difflen = listlen1 - listlen2

    #calc cosine distance
    cosinedist = []
    for i in range(0, len(listpos1)):
        cosinedist.append(distance.cosine(listpos1[i],listpos2[i]))

    npdist = numpy.array(cosinedist)

    #ax = sns.distplot(npdist)
    # seaborn histogram ?histplot
    sns.distplot(npdist, hist=True, kde=False,
                 bins=int(1000), color='blue',
                 hist_kws={'edgecolor': 'black'})

    # Add labels
    plt.title('Acceptable Distance')
    plt.xlabel('cosine distance')
    plt.ylabel('count')
    #plt.tight_layout()
    plt.show()

    print("pos min max mean:", numpy.min(diffpos),numpy.max(diffpos),numpy.mean(diffpos))
    print("len min max mean:", numpy.min(difflen),numpy.max(difflen),numpy.mean(difflen))


    #write distance file each video file
    if len(ofile) > 0:
        fileout = open(ofile, 'w', newline='')
        wr = csv.writer(fileout)
        wr.writerow(['filepath', 'width', 'height', 'fps', 'bitrate', 'profile', 'devmode', 'framecount',
                     'indices', 'position', 'length', 'cosdis', 'target'])

        brheader = False
        index = 0
        with open(incsv1) as csvfile:
            rd = csv.reader(csvfile, delimiter=',')
            for row in rd:
                print(row)
                wrow = []
                if brheader == False:
                    brheader = True
                else:
                    wrow = row
                    wrow.append(npdist[index])
                    wrow.append(target)
                    wr.writerow(wrow)
                    index = index + 1

        fileout.close()

    print('Success!')