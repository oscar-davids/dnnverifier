import csv
import argparse
import numpy

def getvallist(fname):
    with open(fname) as csv_file:
        lval = []
        csv_reader = csv.reader(csv_file, delimiter=',')
        flag = True
        for row in csv_reader:
            print(row)
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
    listvall = getvallist(incsv1)
    listval2 = getvallist(incsv2)

    listpos1 = numpy.array(covertdigitlist(listvall,1))
    listlen1 = numpy.array(covertdigitlist(listvall,2))
    listpos2 = numpy.array(covertdigitlist(listval2,1))
    listlen2 = numpy.array(covertdigitlist(listval2,2))

    diffpos = listpos1 - listpos2
    difflen = listlen1 - listlen2

    maxposlist = numpy.max(diffpos)
    minposlist = numpy.min(diffpos)

    maxlenlist = numpy.max(difflen)
    minlenlist = numpy.min(difflen)

    print("maxpos:", maxposlist, "minpos:", minposlist)
    print("maxlen:", maxlenlist, "minlen:", minlenlist)

    print('Success!')