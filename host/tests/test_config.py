import pytest

from resfuzz.config import Config, ConfigError, from_dict, load


def test_defaults():
    c = Config()
    assert c.target.deploy_dir.startswith("/data")
    assert c.image.arch == "x86_64"


def test_load_toml(tmp_path):
    p = tmp_path / "c.toml"
    p.write_text('out_dir = "r"\n[image]\narch = "aarch64le"\n[fuzz]\nseconds = 5\ncoverage = false\n'
                 '[target]\nspawn = "/data/x"\ndicts = ["a.dict"]\n')
    c = load(p)
    assert (c.out_dir, c.image.arch, c.fuzz.seconds, c.fuzz.coverage, c.target.dicts) == \
        ("r", "aarch64le", 5, False, ["a.dict"])


@pytest.mark.parametrize("d", [
    {"bogus": {}},
    {"fuzz": {"nope": 1}},
    {"fuzz": {"seconds": "10"}},
    {"fuzz": {"coverage": 1}},
    {"image": {"arch": "ppc"}},
    {"target": {"spawn": None}, "fuzz": {"coverage": True}},
    {"fuzz": 3},
])
def test_invalid(d):
    with pytest.raises(ConfigError):
        from_dict(d)
