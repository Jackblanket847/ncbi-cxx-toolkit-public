
virtualenv3.6 .nsmultitest
source .nsmultitest/bin/activate
pip install grid-python

/home/coremake/test/netschedule/multi_test_netschedule.py 4.16.8 $NCBI/c++.metastable/ReleaseMT/bin/netscheduled

deactivate
rm -rf .nsmultitest

