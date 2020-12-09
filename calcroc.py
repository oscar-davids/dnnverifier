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
from pathlib import Path
import numpy as np
import matplotlib.pyplot as pp
import sklearn.metrics as metrics
from sklearn.metrics import accuracy_score,precision_score, recall_score

def perf_measure(y_actual, y_hat):
    TP = 0
    FP = 0
    TN = 0
    FN = 0

    for i in range(len(y_hat)):
        if y_actual[i]==y_hat[i]==1:
           TP += 1
        if y_hat[i]==1 and y_actual[i]!=y_hat[i]:
           FP += 1
        if y_actual[i]==y_hat[i]==0:
           TN += 1
        if y_hat[i]==0 and y_actual[i]!=y_hat[i]:
           FN += 1

    return(TP, FP, TN, FN)

def main():
    parser = argparse.ArgumentParser(description='psnr calculation parser.')
    parser.add_argument('--infile', type=str, help='select directory', default='psnrresult_total.csv')
    parser.add_argument('--feature', type=str, help='select directory', default='psnr')
    args = parser.parse_args()

    infile = args.infile
    feature = args.feature

    if infile == None:
        print("select psnr result csv file")
        return

    scores = []
    lables = []
    if infile != None:
        with open(infile) as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                #print(row['psnr'], row['target'])
                scores.append( float(row[feature]))
                lables.append( 1- int(row['target']))

    smax = float(max(scores))

    #ascores = np.array(scores) / smax
    ascores = np.array(scores)
    alable = np.array(lables)


    afpr, atpr, thresholds = metrics.roc_curve(alable, ascores)

    arocauc = metrics.auc(afpr, atpr)

    pp.title("Receiver Operating Characteristic")
    pp.xlabel("False Positive Rate(1 - Specificity)")
    pp.ylabel("True Positive Rate(Sensitivity)")
    pp.xscale("log")
    pp.plot(afpr, atpr, "b", label="(AUC = %0.2f)" % arocauc)
    pp.xlim([0.0, 0.001])
    #pp.plot([0, 0.1], [0, 1], "y--")
    #pp.plot([0, 1], [0, 1], "r--")
    pp.legend(loc="lower right")
    pp.show()

    '''
    pp.figure()
    pp.plot(1.0 - atpr, thresholds, marker='*', label='tpr')
    pp.plot(afpr, thresholds, marker='o', label='fpr')
    pp.legend()
    pp.xlim([0, 1])
    pp.ylim([0, 1])
    pp.xlabel('thresh')
    pp.ylabel('far/fpr')
    pp.title(' thresh - far/fpr')
    pp.show()
    '''

    for threval in np.arange(0.0001, 0.1, 0.0001):
        predictval = (ascores > threval)

        TP, FP, TN, FN = perf_measure(alable, predictval)
        FACC = (TP + TN) / float(len(alable))
        TPR = TP / float(TP + FN)
        FPR = FP / float(FP + TN)
        print("Threshold: %.5f Accuracy :%.5f  TPR:%.5f FPR:%.5f" % (threval, FACC, TPR, FPR))

        '''
        FACC = accuracy_score(alable, predictval)
        FFAR = precision_score(alable, predictval)
        FTPR = recall_score(alable, predictval)
        #ftar =  precision_score(alable, predictval)
        #ffar = recall_score(alable, predictval)
        #print("Threshold: %.5f Accuracy : %.5f  FAR: %.5f FRR: %.5f" %(threval ,FACC, FFAR,FFRR))
        '''

    print("task complete!")


if __name__== "__main__":
    main()
