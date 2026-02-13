import joblib
import polars as pl
from sklearn.tree import DecisionTreeClassifier

data = pl.read_csv("data_train.csv")

X = data[["roll", "pitch", "gz", "ax", "ay", "az"]]
y = data["anomaly"]

X = (X * 1000).cast(pl.Int16)

clf = DecisionTreeClassifier()
clf.fit(X, y)

print(clf.score(X, y))

joblib.dump(clf, "model.gz")
