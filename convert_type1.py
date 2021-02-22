import argparse
import datetime
import pandas as pd

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--infile", default="d:/data-new-metrics.csv", help="csv file to calculate confusion matrix.")
    args = parser.parse_args()
    infile = args.infile

    label = []
    ypred = []

    FEATURES_FULL = ['id','title','attack','dimension_x','dimension_y','fps','path','pixels','size','size_dimension_ratio',
                   'size_dimension_ratio',
                   'temporal_dct-mean',
                   'temporal_gaussian_mse-mean',
                   'temporal_gaussian_difference-mean',
                   'temporal_threshold_gaussian_difference-mean',
                   'temporal_histogram_distance-mean'
                   ]

    outcsv = "train_feed" + datetime.datetime.now().strftime("%m%d%H%M") + ".csv"

    data_df = pd.read_csv(infile, nrows=None)

    dataneed = data_df[FEATURES_FULL]
    dataneed.to_csv(outcsv)


