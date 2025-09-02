#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "emlearn",
#     "scikit-learn",
#     "joblib",
#     "setuptools",
#     "jinja2",
# ]
# ///
from pathlib import Path
import emlearn
import sys
import joblib
import jinja2

from sklearn.mixture import GaussianMixture

estimator: GaussianMixture = joblib.load(sys.argv[1])

features = estimator.n_features_in_
components = len(estimator.means_)

cmodel = emlearn.convert(estimator, method="loadable")
src = cmodel.save("g_emlearn_model")


template_str = """
{{ src }}

int32_t detector_emlearn_detect_impl(const float * buf, float * score) {
    static float probs[{{ components }}];
    EmlError status;

    status = g_emlearn_model_score(buf, {{ features }}, probs, score);

    return 0;
}
"""

env = jinja2.Environment()
template = env.from_string(template_str)
rendered = template.render(src=src, components=components, features=features)

Path(sys.argv[2]).write_text(rendered)
