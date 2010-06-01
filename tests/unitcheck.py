from unittest import *

class CheckResult (TestResult):

    successes = ()			# Immutable

    def addSuccess(self, test):
	if not self.successes:
	    self.successes = []
	self.successes.append(test)


class _CheckCase:

    def defaultTestResult(self):
	return CheckResult()


class CheckCase (_CheckCase, TestCase):
    pass


class FunctionCheckCase (_CheckCase, FunctionTestCase):
    pass
