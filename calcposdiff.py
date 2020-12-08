import csv
import argparse
import numpy
from scipy.spatial import distance
import seaborn as sns
import matplotlib.pyplot as plt


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
    args = parser.parse_args()
    incsv1 = args.inf1
    incsv2 = args.inf2

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
    bnpdist = npdist < 0.0012
    count = numpy.count_nonzero(bnpdist)

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

    print(numpy.min(npdist))
    print(numpy.max(npdist))

    maxposlist = numpy.max(diffpos)
    minposlist = numpy.min(diffpos)

    maxlenlist = numpy.max(difflen)
    minlenlist = numpy.min(difflen)

    print("maxpos:", maxposlist, "minpos:", minposlist)
    print("maxlen:", maxlenlist, "minlen:", minlenlist)

    print('Success!')