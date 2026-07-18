from setuptools import find_packages, setup
import os
from glob import glob # 記得加上這行

package_name = 'v5_bringup'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # ★ 加上這行，讓 ROS 2 在編譯時把 launch 檔案複製過去
        (os.path.join('share', package_name, 'launch'), glob(os.path.join('launch', '*launch.[pxy][yma]*'))),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='yilerler',
    maintainer_email='yilerler@todo.todo',
    description='V5.2 System Bringup Launch Files',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
        ],
    },
)