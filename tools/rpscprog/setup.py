from setuptools import setup

with open('requirements.txt', 'r') as fp:
  installrequires = fp.readlines()

setup(
    name='rpscprog',
    description='Programming and configuration interface for the Royal Palms Shuffleboard Billy Bass Wall',
    version='0.1.0',
    packages=['rpscprog'],
    install_requires=installrequires,
    entry_points={
        'console_scripts': [
            'rpscprog = rpscprog.__main__:cli',
        ],
    },
)
