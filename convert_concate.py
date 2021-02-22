import argparse
import datetime
import pandas as pd

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="d:/new_train.csv", help="csv file to calculate confusion matrix.")
    args = parser.parse_args()
    outfile = args.out

    data_df0 = pd.read_csv("d:/tmp/train_feed_org.csv", nrows=None)
    data_df1 = pd.read_csv("d:/tmp/01_out.csv", nrows=None)
    data_df2 = pd.read_csv("d:/tmp/02_out.csv", nrows=None)
    data_df3 = pd.read_csv("d:/tmp/03_out.csv", nrows=None)
    data_df4 = pd.read_csv("d:/tmp/04_out.csv", nrows=None)
    data_df5 = pd.read_csv("d:/tmp/05_out.csv", nrows=None)

    frames = [data_df0, data_df1, data_df2, data_df3, data_df4, data_df5]

    result = pd.concat(frames)
    result.to_csv(outfile)

