class QioException(Exception):
	pass

class QioInvalidUrl(QioException):
	pass

class QioInvalidHandshake(QioException):
	pass

class QioInvalidMessage(QioException):
	pass