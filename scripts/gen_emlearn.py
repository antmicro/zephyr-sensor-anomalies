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

from sklearn.covariance import EllipticEnvelope
from sklearn.mixture import GaussianMixture
from sklearn.tree import DecisionTreeClassifier

THRESHOLD = float(sys.argv[3]) if len(sys.argv) > 3 else 50.0


def prepare_gaussian_mixture(clf: GaussianMixture):
    features = clf.n_features_in_
    components = len(clf.means_)

    cmodel = emlearn.convert(clf, method="loadable")
    src = cmodel.save("g_emlearn_model")

    template_str = """
    {{ src }}

    int32_t detector_emlearn_detect_impl(const float * buf, float * score) {
        static float probs[{{ components }}];
        EmlError status;

        status = g_emlearn_model_score(buf, {{ features }}, probs, score);

        *score = *score > {{ threshold }};

        return 0;
    }
    """

    env = jinja2.Environment()
    template = env.from_string(template_str)
    rendered = template.render(
        src=src, components=components, features=features, threshold=THRESHOLD
    )

    return rendered


def prepare_elliptic_envelope(clf: EllipticEnvelope):
    features = clf.n_features_in_

    cmodel = emlearn.convert(clf, method="inline")
    src = cmodel.save("g_emlearn_model")

    template_str = """
    {{ src }}

    int32_t detector_emlearn_detect_impl(const float * buf, float * score) {
        float dist;
        *score = !eml_elliptic_envelope_predict(&g_emlearn_model_classifier, buf, {{ features }}, &dist);
        return 0;
    }
    """

    env = jinja2.Environment()
    template = env.from_string(template_str)
    rendered = template.render(src=src, features=features, threshold=THRESHOLD)

    return rendered


def prepare_decision_tree(clf: DecisionTreeClassifier):
    features = clf.n_features_in_

    cmodel = emlearn.convert(clf, method="inline")
    src = cmodel.save("g_emlearn_model")

    template_str = """
    {{ src }}

    int32_t detector_emlearn_detect_impl(const float * buf, float * score) {
        int16_t qbuf[{{ features }}];
        for (int i = 0; i < {{ features }}; i++) {
            qbuf[i] = buf[i] * 1000;
        }
        float pred = g_emlearn_model_predict(qbuf, {{ features }});
        *score = 1 - pred;
        return 0;
    }
    """

    env = jinja2.Environment()
    template = env.from_string(template_str)
    rendered = template.render(src=src, features=features)

    return rendered


def main():
    template_map = {
        GaussianMixture: prepare_gaussian_mixture,
        EllipticEnvelope: prepare_elliptic_envelope,
        DecisionTreeClassifier: prepare_decision_tree,
    }

    estimator = joblib.load(sys.argv[1])

    rendered = template_map[type(estimator)](estimator)

    print(rendered)

    Path(sys.argv[2]).write_text(rendered)


if __name__ == "__main__":
    main()
