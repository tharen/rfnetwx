"""
Setup scripts for PyRFNetX.
"""

import os
from setuptools import setup, find_packages

here = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join(here, 'README.MD'), encoding='utf-8') as f:
    long_description = f.read()

setup(
    name='pyrfnetx'
    , version='0.0.1b0'
    , description='A package for managing distributed sensors and hosting data.'
    , long_description=long_description
    , url='https://github.com/tharen/pyrfnetx'
    , author='Tod Haren'
    , author_email='no-reply@gmail.com'
    , license='MIT'
    , classifiers=[
        'Development Staus :: 3 - Alpha'
        , 'License :: OSI Approved :: MIT License'
        ]
    , packages=find_packages(exclude=['contrib',])
    , install_requires=[
            'pyserial', 'flask'
            , 'matplotlib', 'bokeh', 'pandas', 'numpy'
            ]
    , extra_requires={
        'dev': []
        , 'test': ['pytest',]
        }
    , package_data={}
    , data_files=[]
    , entry_points={
        'consol_scripts': [
            'rfnetx = rfnetx.__main__:main'
            ]
        }
    )
