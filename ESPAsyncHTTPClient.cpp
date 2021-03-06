#include <String.h>

#define DEBUG(...) {}

class ByteString: public String {
public:
	ByteString(void *data, size_t len) :
			String() {
		copy(data, len);
	}

	ByteString() :
			String() {
	}

	String& copy(const void *data, unsigned int length) {
		if (!reserve(length)) {
			invalidate();
			return *this;
		}
		len = length;
		memcpy(buffer, data, length);
		buffer[length] = 0;
		return *this;
	}
};

/**
 * Asynchronous HTTP Client
 */
struct AsyncHTTPClient {
	AsyncClient *aClient = NULL;

	bool initialized = false;
	String protocol;
	String base64Authorization;
	String host;
	int port;
	String uri;
	String request;

	ByteString response;
	int statusCode;
	void (*onSuccess)();
	void (*onFail)(String);

	void initialize(String url) {
		// check for : (http: or https:
		int index = url.indexOf(':');
		if(index < 0) {
			initialized = false;	// This is not a URL
		}

		protocol = url.substring(0, index);
		DEBUG(protocol);
		url.remove(0, (index + 3)); // remove http:// or https://

		index = url.indexOf('/');
		String hostPart = url.substring(0, index);
		DEBUG(hostPart);
		url.remove(0, index); // remove hostPart part

		// get Authorization
		index = hostPart.indexOf('@');

		if(index >= 0) {
			// auth info
			String auth = hostPart.substring(0, index);
			hostPart.remove(0, index + 1); // remove auth part including @
			base64Authorization = base64::encode(auth);
		}

		// get port
		port = 80;	//Default
		index = hostPart.indexOf(':');
		if(index >= 0) {
			host = hostPart.substring(0, index); // hostname
			host.remove(0, (index + 1)); // remove hostname + :
			DEBUG(host);
			port = host.toInt(); // get port
			DEBUG(port);
		} else {
			host = hostPart;
			DEBUG(host);
		}
		uri = url;
		if (protocol != "http") {
			initialized = false;
		}

		DEBUG(initialized);
		request = "GET " + uri + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";

		DEBUG(request);
		initialized = true;
	}

	int getStatusCode() { return statusCode; }

	String getBody() {
		if (statusCode == 200) {
			int bodyStart = response.indexOf("\r\n\r\n") + 4;
			return response.substring(bodyStart);
		} else {
			return "";
		}
	}

	static void clientError(void *arg, AsyncClient *client, int error) {
		DEBUG("Connect Error");
		AsyncHTTPClient *self = (AsyncHTTPClient*)arg;
		self->onFail("Connection error");
		self->aClient = NULL;
		delete client;
	}

	static void clientDisconnect(void *arg, AsyncClient *client) {
		DEBUG("Disconnected");
		AsyncHTTPClient *self = (AsyncHTTPClient*)arg;
		self->aClient = NULL;
		delete client;
	}

	static void clientData(void *arg, AsyncClient *client, void *data, size_t len) {
		DEBUG("Got response");

		AsyncHTTPClient *self = (AsyncHTTPClient*)arg;
		self->response = ByteString(data, len);
		String status = self->response.substring(9, 12);
		self->statusCode = atoi(status.c_str());
		DEBUG(status.c_str());

		if (self->statusCode == 200) {
			self->onSuccess();
		} else {
			self->onFail("Failed with code " + status);
		}
	}

	static void clientConnect(void *arg, AsyncClient *client) {
		DEBUG("Connected");

		AsyncHTTPClient *self = (AsyncHTTPClient*)arg;

		self->response.copy("", 0);
		self->statusCode = -1;

		// Clear oneError handler
		self->aClient->onError(NULL, NULL);

		// Set disconnect handler
		client->onDisconnect(clientDisconnect, self);

		client->onData(clientData, self);

		//send the request
		client->write(self->request.c_str());
	}

	void makeRequest(void (*success)(), void (*fail)(String msg)) {
		onFail = fail;

		if (!initialized) {
			fail("Not initialized");
			return;
		}

		if (aClient) { //client already exists
			fail("Call taking forever");
			return;
		}

		aClient = new AsyncClient();
		if (!aClient) { //could not allocate client
			fail("Out of memory");
			return;
		}

		onSuccess = success;

		aClient->onError(clientError, this);

		aClient->onConnect(clientConnect, this);

		if (!aClient->connect(host.c_str(), port)) {
			DEBUG("Connect Fail");
			fail("Connection failed");
			AsyncClient * client = aClient;
			aClient = NULL;
			delete client;
		}
	}
};

