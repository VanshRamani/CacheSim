@echo off
REM Copy trace files to expected format for the simulator
REM False Sharing Test
copy test_cases\falsesharing_core0.trace test_cases\falsesharing_proc0.trace
copy test_cases\falsesharing_core1.trace test_cases\falsesharing_proc1.trace
copy test_cases\falsesharing_core2.trace test_cases\falsesharing_proc2.trace
copy test_cases\falsesharing_core3.trace test_cases\falsesharing_proc3.trace

REM Ping-Pong Test
copy test_cases\pingpong_core0.trace test_cases\pingpong_proc0.trace
copy test_cases\pingpong_core1.trace test_cases\pingpong_proc1.trace
copy test_cases\pingpong_core2.trace test_cases\pingpong_proc2.trace
copy test_cases\pingpong_core3.trace test_cases\pingpong_proc3.trace

REM Cache-to-Cache Transfer Test
copy test_cases\c2c_core0.trace test_cases\c2c_proc0.trace
copy test_cases\c2c_core1.trace test_cases\c2c_proc1.trace
copy test_cases\c2c_core2.trace test_cases\c2c_proc2.trace
copy test_cases\c2c_core3.trace test_cases\c2c_proc3.trace

REM Hot Spot Test
copy test_cases\hotspot_core0.trace test_cases\hotspot_proc0.trace
copy test_cases\hotspot_core1.trace test_cases\hotspot_proc1.trace
copy test_cases\hotspot_core2.trace test_cases\hotspot_proc2.trace
copy test_cases\hotspot_core3.trace test_cases\hotspot_proc3.trace

REM Private vs Shared Data Test
copy test_cases\private_shared_core0.trace test_cases\private_shared_proc0.trace
copy test_cases\private_shared_core1.trace test_cases\private_shared_proc1.trace
copy test_cases\private_shared_core2.trace test_cases\private_shared_proc2.trace
copy test_cases\private_shared_core3.trace test_cases\private_shared_proc3.trace

REM Run the tests with typical cache parameters
REM s=6 (64 sets), E=2 (2-way associative), b=5 (32-byte blocks)
.\bin\L1simulate -t test_cases/falsesharing -s 6 -E 2 -b 5 -o test_cases/output/falsesharing_output.txt
.\bin\L1simulate -t test_cases/pingpong -s 6 -E 2 -b 5 -o test_cases/output/pingpong_output.txt
.\bin\L1simulate -t test_cases/c2c -s 6 -E 2 -b 5 -o test_cases/output/c2c_output.txt
.\bin\L1simulate -t test_cases/hotspot -s 6 -E 2 -b 5 -o test_cases/output/hotspot_output.txt
.\bin\L1simulate -t test_cases/private_shared -s 6 -E 2 -b 5 -o test_cases/output/private_shared_output.txt

echo All tests completed. Results are in test_cases/output/ directory. 