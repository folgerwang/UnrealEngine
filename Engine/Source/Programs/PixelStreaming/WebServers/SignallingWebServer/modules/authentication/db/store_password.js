// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
//
// Usage: node store_password --username <USERNAME> --password <PASSWORD>
//
// There is an optional paramter '--usersFile <USERS_FILEPATH>' that can be used to specify a 
// different location for the file to save the users to. The default location is './users.json'

const argv = require('yargs').argv;
const fs = require('fs');
const bcrypt = require('bcrypt');

var username, password;
var usersFile = './users.json'

const STORE_PLAINTEXT_PASSWORD = false;

try {
	if(typeof argv.username != 'undefined'){
		username = argv.username.toString();
    }
	
	if(typeof argv.password != 'undefined'){
		password = argv.password;
    }

    if(typeof argv.usersFile != 'undefined'){
		usersFile = argv.usersFile;
    }
} catch (e) {
    console.error(e);
    process.exit(2);
}

if(username && password){
	let existingAccounts = [];
	if (fs.existsSync(usersFile)) {
		console.log(`File '${usersFile}' exists, reading file`)
		var content = fs.readFileSync(usersFile, 'utf8');
		try{
			existingAccounts = JSON.parse(content);
		}
		catch(e){
			console.error(`Existing file '${usersFile}', has invalid JSON: ${e}`);
		}
	}

	var existingUser = existingAccounts.find( u => u.username == username)
	if(existingUser){
		console.log(`User '${username}', already exists, updating password`)
		existingUser.passwordHash = generatePasswordHash(password)
		if(STORE_PLAINTEXT_PASSWORD)
			existingUser.password = password;
		else if (existingUser.password)
			delete existingUser.password;

	} else {
		console.log(`Adding new user '${username}'`)
		let newUser = {
			id: existingAccounts.length + 1,
			username: username,
			passwordHash: generatePasswordHash(password)
		}
		if(STORE_PLAINTEXT_PASSWORD)
			newUser.password = password;

		existingAccounts.push(newUser);
	}

	console.log(`Writing updated users to '${usersFile}'`);
	var newContent = JSON.stringify(existingAccounts);
	fs.writeFileSync(usersFile, newContent);
} else {
	console.log(`Please pass in both username (${username}) and password (${password}) please`);
}

function generatePasswordHash(pass){
	return bcrypt.hashSync(pass, 12)
}