from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'v5_it_gateway'

setup(
    name=package_name,
    version='5.2.4',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools', 'firebase-admin'],
    zip_safe=True,
    maintainer='Joshua Lin',
    maintainer_email='your_email@example.com',
    description='The Northbound IT Gateway for V5 Safety Controller',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'it_gateway_node = v5_it_gateway.it_gateway_node:main'
        ],
    },
)