// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

//-- Server side logic. Serves pixel streaming WebRTC-based page, proxies data back to WebRTC proxy --//

var express = require('express');
var app = express();

const fs = require('fs');
const path = require('path');
const querystring = require('querystring');
const bodyParser = require('body-parser');
const logging = require('./modules/logging.js');
logging.RegisterConsoleLogger();

// Command line argument --configFile needs to be checked before loading the config, all other command line arguments are dealt with through the config object

const defaultConfig = {
	UseFrontend: false,
	UseMatchmaker: false,
	UseHTTPS: false,
	UseAuthentication: false,
	LogToFile: true,
	HomepageFile: 'player.htm',
	AdditionalRoutes: new Map()
};

const argv = require('yargs').argv;
var configFile = (typeof argv.configFile != 'undefined') ? argv.configFile.toString() : '.\\config.json';
const config = require('./modules/config.js').init(configFile, defaultConfig)

if (config.LogToFile) {
	logging.RegisterFileLogger('./logs');
}

console.log("Config: " + JSON.stringify(config, null, '\t'))

var http = require('http').Server(app);

if(config.UseHTTPS){
	//HTTPS certificate details
	const options = {
		key: fs.readFileSync(path.join(__dirname, './certificates/client-key.pem')),
		cert: fs.readFileSync(path.join(__dirname, './certificates/client-cert.pem'))
	};

	var https = require('https').Server(options, app);
	var io = require('socket.io')(https);
} else {
	var io = require('socket.io')(http);
}

//If not using authetication then just move on to the next function/middleware
var isAuthenticated = redirectUrl => function(req, res, next){ return next(); }

if(config.UseAuthentication && config.UseHTTPS){
	var passport = require('passport');
	require('./modules/authentication').init(app);
	// Replace the isAuthenticated with the one setup on passport module
	isAuthenticated = passport.authenticationMiddleware ? passport.authenticationMiddleware : isAuthenticated
} else if(config.UseAuthentication && !config.UseHTTPS) {
	console.log('ERROR: Trying to use authentication without using HTTPS, this is not allowed and so authentication will NOT be turned on, please turn on HTTPS to turn on authentication');
}

const helmet = require('helmet');
var hsts = require('hsts');
var net = require('net');

var FRONTEND_WEBSERVER = 'https://localhost';
if(config.UseFrontend){
	var httpPort = 3000;
	var httpsPort = 8000;
	
	//Required for self signed certs otherwise just get an error back when sending request to frontend see https://stackoverflow.com/a/35633993
	process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0"

	const httpsClient = require('./modules/httpsClient.js');
	var webRequest = new httpsClient();
} else {
	var httpPort = 80;
	var httpsPort = 443;
}

var proxyPort = 8888; // port to listen to WebRTC proxy connections
var proxyBuffer = new Buffer(0);

var matchmakerAddress = '127.0.0.1';
var matchmakerPort = 9999;

var gameSessionId;
var userSessionId;
var serverPublicIp;

//Example of STUN server setting
//let clientConfig = {peerConnectionOptions: { 'iceServers': [{'urls': ['stun:34.250.222.95:19302']}] }};
var clientConfig = {peerConnectionOptions: {}};

// Parse public server address from command line
// --publicIp <public address>
try {
	if(typeof config.publicIp != 'undefined'){
		serverPublicIp = config.publicIp.toString();
    }
	
	if(typeof config.httpPort != 'undefined'){
		httpPort = config.httpPort;
    }
	
	if(typeof config.httpsPort != 'undefined'){
		httpsPort = config.httpsPort;
    }
	
	if(typeof config.proxyPort != 'undefined'){
		proxyPort = config.proxyPort;
    }

	if(typeof config.frontendUrl != 'undefined'){
		FRONTEND_WEBSERVER = config.frontendUrl;
    }
	
	if(typeof config.peerConnectionOptions != 'undefined'){
		clientConfig.peerConnectionOptions = JSON.parse(config.peerConnectionOptions);
		console.log(`peerConnectionOptions = ${JSON.stringify(clientConfig.peerConnectionOptions)}`);
	}
	
	if (typeof config.matchmakerAddress != 'undefined') {
		matchmakerAddress = config.matchmakerAddress;
	}
	
	if (typeof config.matchmakerPort != 'undefined') {
		matchmakerPort = config.matchmakerPort;
	}
} catch (e) {
    console.error(e);
    process.exit(2);
}

if(config.UseHTTPS){
	app.use(helmet());

	app.use(hsts({
		maxAge: 15552000  // 180 days in seconds
	}));
	
	//Setup http -> https redirect
	console.log('Redirecting http->https');
	app.use(function (req, res, next) {
		if (!req.secure) {
			if(req.get('Host')){
				var hostAddressParts = req.get('Host').split(':');
				var hostAddress = hostAddressParts[0];
				if(httpsPort != 443) {
					hostAddress = `${hostAddress}:${httpsPort}`;
				}
				return res.redirect(['https://', hostAddress, req.originalUrl].join(''));
			} else {
				console.log(`ERROR unable to get host name from header. Requestor ${req.ip}, url path: '${req.originalUrl}', available headers ${JSON.stringify(req.headers)}`);
				return res.status(400).send('Bad Request');
			}
		}
		next();
	});
}

sendGameSessionData();

//Setup folders
app.use(express.static(path.join(__dirname, '/public')))
app.use('/images', express.static(path.join(__dirname, './images')))
app.use('/scripts', [isAuthenticated('/login'), express.static(path.join(__dirname, '/scripts'))]);
app.use('/', [isAuthenticated('/login'), express.static(path.join(__dirname, '/custom_html'))])

try{
	for (var property in config.AdditionalRoutes) {
	    if (config.AdditionalRoutes.hasOwnProperty(property)) {
	    	console.log(`Adding additional routes "${property}" -> "${config.AdditionalRoutes[property]}"`)
	    	app.use(property, [isAuthenticated('/login'), express.static(path.join(__dirname, config.AdditionalRoutes[property]))]);
	    }
	}
} catch(err) {
	console.log(`Error reading config.AdditionalRoutes: ${err}`)
}


app.get('/', isAuthenticated('/login'), function(req, res){
	homepageFile = (typeof config.HomepageFile != 'undefined' && config.HomepageFile != '') ? config.HomepageFile.toString() : defaultConfig.HomepageFile;
	homepageFilePath = path.join(__dirname, homepageFile)

	fs.access(homepageFilePath, (err) => {
		if (err) {
			console.log('Unable to locate file ' + homepageFilePath)
			res.status(404).send('Unable to locate file ' + homepageFile);
		}
		else {
			res.sendFile(homepageFilePath);
		}
	});
});

//Setup the login page if we are using authentication
if(config.UseAuthentication){
	app.get('/login', function(req, res){
		res.sendFile(__dirname + '/login.htm');
	});

	// create application/x-www-form-urlencoded parser
	var urlencodedParser = bodyParser.urlencoded({ extended: false })

	//login page form data is posted here
	app.post('/login', 
		urlencodedParser, 
		passport.authenticate('local', { failureRedirect: '/login' }), 
		function(req, res){
			//On success try to redirect to the page that they originally tired to get to, default to '/' if no redirect was found
			var redirectTo = req.session.redirectTo ? req.session.redirectTo : '/';
			delete req.session.redirectTo;
			console.log(`Redirecting to: '${redirectTo}'`);
			res.redirect(redirectTo);
		}
	);
}

/*
app.get('/:sessionId', isAuthenticated('/login'), function(req, res){
	let sessionId = req.params.sessionId;
	console.log(sessionId);
	
	//For now don't verify session id is valid, just send player.htm if they get the right server
  res.sendFile(__dirname + '/player.htm');
});
*/

/* 
app.get('/custom_html/:htmlFilename', isAuthenticated('/login'), function(req, res){
	let htmlFilename = req.params.htmlFilename;
	
	let htmlPathname = __dirname + '/custom_html/' + htmlFilename;

	console.log(htmlPathname);
	fs.access(htmlPathname, (err) => {
		if (err) {
			res.status(404).send('Unable to locate file ' + htmlPathname);
		}
		else {
			res.sendFile(htmlPathname);
		}
	});
});
*/

let clients = []; // either web-browsers or native webrtc receivers
let nextClientId = 100;

let proxySocket;

function cleanUpProxyConnection() {
	if(proxySocket){
		proxySocket.end();
		proxySocket = undefined;
		proxyBuffer = new Buffer(0);
		// make a copy of `clients` array as it will be modified in the loop
		let clientsCopy = clients.slice();
		clientsCopy.forEach(function (c) {
			c.ws.disconnect();
		});
	}
}

let proxyListener = net.createServer(function(socket) {
	// 'connection' listener
	console.log('proxy connected');

	socket.setNoDelay();

	socket.on('data', function (data) {
		proxyBuffer = Buffer.concat([proxyBuffer, data]);

		// WebRTC proxy uses json messages instead of binary blob so need to read messages differently
		while (handleProxyMessage(socket)) { }
	});
    
	socket.on('end', function () {
		console.log('proxy connection end');
		cleanUpProxyConnection();
	});

	socket.on('disconnect', function () {
		console.log('proxy disconnected');
		cleanUpProxyConnection();
	});
    
	socket.on('close', function() {
		sendServerDisconnect();
		console.log('proxy connection closed');
		proxySocket = undefined;
	});
	
	socket.on('error', function (error) {
		console.log(`proxy connection error ${JSON.stringify(error)}`);
		cleanUpProxyConnection();
	});

	proxySocket = socket;

	sendConfigToProxy();
});
    
proxyListener.maxConnections = 1;
proxyListener.listen(proxyPort, () => {
	console.log('Listening to proxy connections on: ' + proxyPort);
});

// Must be kept in sync with PixelStreamingProtocol::EProxyToCirrusMsg C++ enum.
const EProxyToCirrusMsg = {
	answer: 0, // [msgId:1][clientId:4][size:4][string:size]
	iceCandidate: 1, // [msgId:1][clientId:4][size:4][string:size]
	disconnectClient: 2 // [msgId:1][clientId:4]
}

// Must be kept in sync with PixelStreamingProtocol::ECirrusToProxyMsg C++ enum.
const ECirrusToProxyMsg = {
	offer: 0, // [msgId: 1][clientId:4][size:4][string:size]
	iceCandidate: 1, // [msgId:1][clientId:4][size:4][string:size]
	clientDisconnected: 2, // [msgId:1][clientId:4]
	config: 3 // [msgId:1][size:4][config:size]
}

function readJsonMsg(consumed) {
	// format: [size:4][string:size]
	if (proxyBuffer.length < consumed + 4)
		return [0, ""];
	let msgSize = proxyBuffer.readUInt32LE(consumed);
	consumed += 4;
	if (proxyBuffer.length < consumed + msgSize)
		return [0, ""];
	let msg = proxyBuffer.toString('ascii', consumed, consumed + msgSize);
	consumed += msgSize;
	return [consumed, JSON.parse(msg)];
}

function handleProxyMessage(socket) {
	// msgId
	if(proxyBuffer.length == 0)
		return false;
	let msgId = proxyBuffer.readUInt8(0);
	let consumed = 1;

	// clientId
	if (proxyBuffer.length < consumed + 4)
		return false;
	let clientId = proxyBuffer.readUInt32LE(consumed);
	consumed += 4;

	let client = clients.find(function(c) { return c.id == clientId; });
	if (!client) {
		// Client is likely no longer connected, but this can also occur if bad data is recieved, this can not be validated as yet so assume former
		console.error(`proxy message ${msgId}: client ${clientId} not found. Check proxy->cirrus protocol consistency`);
	}

	switch (msgId) {
		case EProxyToCirrusMsg.answer: // fall through
		case EProxyToCirrusMsg.iceCandidate:
			let [localConsumed, msg] = readJsonMsg(consumed);
			if (localConsumed == 0)
				return false;
			consumed = localConsumed;

			if(client){
				switch (msgId)
				{
					case EProxyToCirrusMsg.answer:
						console.log(`answer -> client ${clientId}`);
						client.ws.emit('webrtc-answer', msg);
						break;
					case EProxyToCirrusMsg.iceCandidate:
						console.log(`ICE candidate -> client ${clientId}`);
						client.ws.emit('webrtc-ice', msg);
						break;
					default:
						throw "unhandled case, check all \"fall through\" cases from above";
				}
			}

			break;
		case EProxyToCirrusMsg.disconnectClient:
			console.warn(`Proxy instructed to disconnect client ${clientId}`);
			if(client){
				client.ws.onclose = function() {};
				client.ws.disconnect(true);
				let idx = clients.map(function(p) { return p.id; }).indexOf(clientId);
				clients.splice(idx, 1); // remove it
				sendClientDisconnectedToProxy(clientId);
			}
			break;
		default:
			console.error(`Invalid message id ${msgId} from proxy`);
			cleanUpProxyConnection();
			return false;
	}

	proxyBuffer = proxyBuffer.slice(consumed);
	return true;
}

function sendConfigToProxy() {
	// [msgId:1][size:4][string:size]
	if (!proxySocket)
		return false;

	let cfg = {};
	cfg.peerConnectionConfig = clientConfig.peerConnectionOptions;
	let msg = JSON.stringify(cfg);
	console.log(`config to Proxy: ${msg}`);

	let data = new DataView(new ArrayBuffer(1 + 4 + msg.length));
	data.setUint8(0, ECirrusToProxyMsg.config);
	data.setUint32(1, msg.length, true);
	for (let i = 0; i != msg.length; ++i)
		data.setUint8(1 + 4 + i, msg.charCodeAt(i));
	proxySocket.write(Buffer.from(data.buffer));
	return true;
}

function sendClientDisconnectedToProxy(clientId) {
	// [msgId:1][clientId:4]
	if (!proxySocket)
		return;
	let data = new DataView(new ArrayBuffer(1 + 4));
	data.setUint8(0, ECirrusToProxyMsg.clientDisconnected);
	data.setUint32(1, clientId, true);
	proxySocket.write(Buffer.from(data.buffer));
}

function sendStringMsgToProxy(msgId, clientId, msg) {
	// [msgId:1][clientId:4][size:4][string:size]
	if (!proxySocket)
		return false;
	let data = new DataView(new ArrayBuffer(1 + 4 + 4 + msg.length));
	data.setUint8(0, msgId);
	data.setUint32(1, clientId, true);
	data.setUint32(1 + 4, msg.length, true);
	for (let i = 0; i != msg.length; ++i)
		data.setUint8(1 + 4 + 4 + i, msg.charCodeAt(i));
	proxySocket.write(Buffer.from(data.buffer));
	return true;
}

function sendOfferToProxy(clientId, offer) {
	sendStringMsgToProxy(ECirrusToProxyMsg.offer, clientId, offer);
}

function sendIceCandidateToProxy(clientId, iceCandidate) {
	sendStringMsgToProxy(ECirrusToProxyMsg.iceCandidate, clientId, iceCandidate);
}

/**
 * Function that handles the connection to the matchmaker.
 */

if (config.UseMatchmaker) {
	var matchmaker = net.connect(matchmakerPort, matchmakerAddress, () => {
		console.log(`Cirrus connected to Matchmaker ${matchmakerAddress}:${matchmakerPort}`);
		message = {
			type: 'connect',
			address: typeof serverPublicIp === 'undefined' ? '127.0.0.1' : serverPublicIp,
			port: httpPort
		};
		matchmaker.write(JSON.stringify(message));
	});

	matchmaker.on('error', () => {
		console.log('Cirrus disconnected from matchmaker');
	});
}

/**
 * Function that handles an incoming client connection.
 */
function handleNewClient(ws) {
    // NOTE: This needs to be the first thing to be sent
    ws.emit('clientConfig', clientConfig);

    var clientId = ++nextClientId;
    console.log(`client ${clientId} (${ws.request.connection.remoteAddress}) connected`);
    clients.push({ws: ws, id: clientId});

    // Send client counts to all connected clients
    ws.emit('clientCount', {count: clients.length - 1});
	
	clients.forEach(function(c){
		if(c.id == clientId)
			return;
		c.ws.emit('clientCount', {count: clients.length - 1});
	});

    ws.on('userConfig', function(userConfig) {
    	receiveUserConfig(clientId, userConfig, ws);
    });

    /**
    * This is where events received from client are translated
    * and sent on to the proxy socket
    */
    
    ws.on('message', function (msg) {
    	console.error(`client #${clientId}: unexpected msg "${msg}"`);
    });

    ws.on('kick', function(msg){
		// make a copy of `clients` cos the array will be modified in the loop
    	let clientsCopy = clients.slice();
    	clientsCopy.forEach(function(c){
    		if(c.id == clientId)
    			return;
    		console.log('Kicking client ' + c.id);
    		c.ws.disconnect();
    	})
    	ws.emit('clientCount', {count: 0});
    })

    var removeClient = function() {
    	let idx = clients.map(function(c) { return c.ws; }).indexOf(ws);
    	let clientId = clients[idx].id;
    	clients.splice(idx, 1); // remove it
    	sendClientDisconnectedToProxy(clientId);
    	sendClientDisconnectedToFrontend();
		sendClientDisconnectedToMatchmaker();
    }
	
    ws.on('disconnect', function () {
    	console.log(`client ${clientId} disconnected`);
        removeClient();
    });
    
    ws.on('close', function (code, reason) {
    	console.log(`client ${clientId} connection closed: ${code} - ${reason}`);
        removeClient();
    });
    
    ws.on('error', function (err) {
    	console.log(`client ${clientId} connection error: ${err}`);
        removeClient();
    });
};

/**
 * Config data received from the web browser or device native client.
 */
function receiveUserConfig(clientId, userConfigString, ws) {
	console.log(`client ${clientId}: userConfig = ${userConfigString}`);
	userConfig = JSON.parse(userConfigString)

	// Check the sort of data the web browser or device native client will send.
	switch (userConfig.emitData)
	{
		case "ArrayBuffer":
			{
				ws.on('webrtc-offer', function(offer) {
					console.log(`offer <- client ${clientId}`);
					sendOfferToProxy(clientId,  offer);
				});

				ws.on('webrtc-ice', function(candidate) {
					console.log(`ICE candidate <- client ${clientId}`);
					sendIceCandidateToProxy(clientId, candidate);
				});

				ws.on('webrtc-stats', function(stats){
					console.log(`Received webRTC stats from player ID: ${clientId} \r\n${JSON.stringify(stats)}`);
				});

				break;
			}
		case "Array":
			{
				//TODO: this is untested as requires iOS WebRTC integration
				ws.on('webrtc-offer', function(offer) {
					console.log(`offer <- client ${clientId}`);
					sendOfferToProxy(clientId,  offer);
				});

				ws.on('webrtc-ice', function(candidate) {
					console.log(`ICE candidate <- client ${clientId}`);
					sendIceCandidateToProxy(clientId, candidate);
				});

				ws.on('webrtc-stats', function(stats){
					console.log(`Received webRTC stats from player ID: ${clientId} \r\n${JSON.stringify(stats)}`);
				});

				break;
			}
		default:
			{
				console.log(`Unknown user config emit data type ${userConfig.emitData}`);
				break;
			}
	}
}


//IO events
io.on('connection', function (ws) {
	// Reject connection if proxy is not connected
	if (!proxySocket) {
		ws.disconnect();
		return;
	}

    handleNewClient(ws);    
    sendClientConnectedToFrontend();
	sendClientConnectedToMatchmaker();
});

//Setup http and https servers
http.listen(httpPort, function () {
		console.logColor(logging.Green, 'Http listening on *: ' + httpPort);
	});

if(config.UseHTTPS){
	https.listen(httpsPort, function () {
		console.logColor(logging.Green, 'Https listening on *: ' + httpsPort);
	});
}

//Keep trying to send gameSessionId in case the server isn't ready yet
function sendGameSessionData(){
	//If we are not using the frontend web server don't try and make requests to it
	if(!config.UseFrontend)
		return;
	
	webRequest.get(`${FRONTEND_WEBSERVER}/server/requestSessionId`,
		function(response, body) {
			if(response.statusCode === 200){
				gameSessionId = body;
				console.log('SessionId: ' + gameSessionId);
			}
			else{
				console.log('Status code: ' + response.statusCode);
				console.log(body);
			}
		},
		function(err){
			//Repeatedly try in cases where the connection timed out or never connected
			if (err.code === "ECONNRESET") {
				//timeout
				sendGameSessionData();
			} else if(err.code === 'ECONNREFUSED') {
				console.log('Frontend server not running, unable to setup game session');
			} else {
				console.log(err);
			}
		});
}

function sendUserSessionData(serverPort){
	//If we are not using the frontend web server don't try and make requests to it
	if(!config.UseFrontend)
		return;
	
	webRequest.get(`${FRONTEND_WEBSERVER}/server/requestUserSessionId?gameSessionId=${gameSessionId}&serverPort=${serverPort}&appName=${querystring.escape(clientConfig.AppName)}&appDescription=${querystring.escape(clientConfig.AppDescription)}${(typeof serverPublicIp === 'undefined' ? '' : '&serverHost=' + serverPublicIp)}`,
		function(response, body) {
			if(response.statusCode === 410){
				sendUserSessionData(serverPort);
			}else if(response.statusCode === 200){
				userSessionId = body;
				console.log('UserSessionId: ' + userSessionId);
			} else {
				console.log('Status code: ' + response.statusCode);
				console.log(body);
			}
		},
		function(err){
			//Repeatedly try in cases where the connection timed out or never connected
			if (err.code === "ECONNRESET") {
				//timeout
				sendUserSessionData(serverPort);
			} else if(err.code === 'ECONNREFUSED') {
				console.log('Frontend server not running, unable to setup user session');
			} else {
				console.log(err);
			}
		});
}

function sendServerDisconnect(){
	//If we are not using the frontend web server don't try and make requests to it
	if(!config.UseFrontend)
		return;
	
	webRequest.get(`${FRONTEND_WEBSERVER}/server/serverDisconnected?gameSessionId=${gameSessionId}&appName=${querystring.escape(clientConfig.AppName)}`,
		function(response, body) {
			if(response.statusCode === 200){
				console.log('serverDisconnected acknowledged by Frontend');
			} else {
				console.log('Status code: ' + response.statusCode);
				console.log(body);
			}
		},
		function(err){
			//Repeatedly try in cases where the connection timed out or never connected
			if (err.code === "ECONNRESET") {
				//timeout
				sendServerDisconnect();
			} else if(err.code === 'ECONNREFUSED') {
				console.log('Frontend server not running, unable to setup user session');
			} else {
				console.log(err);
			}
		});
}

function sendClientConnectedToFrontend(){
    //If we are not using the frontend web server don't try and make requests to it
    if(!config.UseFrontend)
        return;

    webRequest.get(`${FRONTEND_WEBSERVER}/server/clientConnected?gameSessionId=${gameSessionId}&appName=${querystring.escape(clientConfig.AppName)}`,
		function(response, body) {
		    if(response.statusCode === 200){
		        console.log('clientConnected acknowledged by Frontend');
		    }
		    else{
		        console.log('Status code: ' + response.statusCode);
		        console.log(body);
		    }
		},
		function(err){
		    //Repeatedly try in cases where the connection timed out or never connected
		    if (err.code === "ECONNRESET") {
		        //timeout
		        sendClientConnectedToFrontend();
		    } else if(err.code === 'ECONNREFUSED') {
		        console.log('Frontend server not running, unable to setup game session');
		    } else {
		        console.log(err);
		    }
		});
}

function sendClientDisconnectedToFrontend(){
    //If we are not using the frontend web server don't try and make requests to it
    if(!config.UseFrontend)
        return;

    webRequest.get(`${FRONTEND_WEBSERVER}/server/clientDisconnected?gameSessionId=${gameSessionId}&appName=${querystring.escape(clientConfig.AppName)}`,
		function(response, body) {
		    if(response.statusCode === 200){
		        console.log('clientDisconnected acknowledged by Frontend');
		    }
		    else{
		        console.log('Status code: ' + response.statusCode);
		        console.log(body);
		    }
		},
		function(err){
		    //Repeatedly try in cases where the connection timed out or never connected
		    if (err.code === "ECONNRESET") {
		        //timeout
		        sendClientDisconnectedEvent();
		    } else if(err.code === 'ECONNREFUSED') {
		        console.log('Frontend server not running, unable to setup game session');
		    } else {
		        console.log(err);
		    }
		});
}

// The Matchmaker will not re-direct clients to this Cirrus server if any client
// is connected.
function sendClientConnectedToMatchmaker() {
	if (!config.UseMatchmaker)
		return;
	
	message = {
		type: 'clientConnected'
	};
	matchmaker.write(JSON.stringify(message));
}

// The Matchmaker is interested when nobody is connected to a Cirrus server
// because then it can re-direct clients to this re-cycled Cirrus server.
function sendClientDisconnectedToMatchmaker() {
	if (!config.UseMatchmaker)
		return;

	message = {
		type: 'clientDisconnected'
	};
	matchmaker.write(JSON.stringify(message));
}