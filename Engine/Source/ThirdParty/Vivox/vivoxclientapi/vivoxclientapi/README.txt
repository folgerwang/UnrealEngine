(Copyright(C) 2015 Mercer Road Corp)

[Overview]
    This solution contains two projects:

    1. vivoxclientapi - a set of C++ classes that make integrating game applications simple.
    2. joinchannel - a win32 application that allows one of 8 users to join 1 of 2 two channels.

    Developers are encouraged to interface to the Vivox system using the vivoxclientapi library, which we provide in source form.

    To use the vivoxclientapi library, include "vivoxclientapi.h" in your code, and link against vivoxclientapi.lib. Developers should refrain from changing
    the source to the vivoxclientapi library - we expect to continue to extend it. If it does not meet your specific needs please contact your customer support rep with your need, and we'll address it as quickly as possible.

	Documentation on the specific classes and methods are found in the header files in Javadoc style comments.

[vivoxclientapi.lib/VivoxClientApi Namespace]

    [Interface IClientApiEventHandler]

        The game developer must implement a class that implements IClientApiEventHandler

    [Class DebugClientApiEventHandler]

        The game developer may choose to use DebugClientApiEventHandler as a base implementation of most of IClientApiEventHandler, exclusive of IClientApiEventHandler::InvokeOnUIThread()

    [Template Class WindowsInvokeOnUIThread<T>]

        The game developer can use this template to add an implementation to IClientApiEventHandler::InvokeOnUIThread() to their implementation of IClientApiEventHandler.

        A quick way to get started is to create a class that uses WindowsInvokeOnUIThread<IClientApiEventHandler> as a base class for your application class.

	[Class ClientConnection]

		An application needs on and only one instance of this class. This class is used to connect to the Vivox service, login, and join channels.

[joinchannel.exe]

	[class JoinChannelApp]

		The class JoinChannelApp is a sample implementation of an application class that implements IClientApiEventHandler, and calls ClientConnection methods to 
		initialize, connect, login and join and leave channels.

	[file joinchannel.cpp]

		This the implementation of "main" for a windows application. There are a handful of integration points with the JoinChannel class, and they can be located by
		looking for code between the keywords "BEGINVIVOX" and "ENDVIVOX"

[Questions/Comments/Requests]

	Please send an e-mail to developer@vivox.com for all your developer questions, comments, and requests.