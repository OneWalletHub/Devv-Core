from os.path import join as pjoin
import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="devv-cli",
    version="0.0.1",
    author="Shawn McKenney",
    author_email="shawn.mckenney@emmion.com",
    description="Python tools for the devv network",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://devv.io",
    packages=setuptools.find_packages(),
    scripts=[pjoin('bin','devv-cli')],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
)
