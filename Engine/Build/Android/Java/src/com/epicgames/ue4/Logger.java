package com.epicgames.ue4;

import android.util.Log;

public class Logger
{
	public interface ILoggerCallback
	{
		void LoggerCallback(String Level, String Tag, String Message);
	}

	private static ILoggerCallback mCallback = null;
	private String mTag;
	
	private static boolean bAllowLogging			= true;
	private static boolean bAllowExceptionLogging	= true;

	public static void RegisterCallback(ILoggerCallback callback)
	{
		mCallback = callback;
	}

	public static void SuppressLogs ()
	{
		bAllowLogging = bAllowExceptionLogging = false;
	}

	public Logger(String Tag)
	{
		mTag = Tag;
	}
	
	public void debug(String Message)
	{
		if (bAllowLogging)
		{
			Log.d(mTag, Message);
		}
		if (mCallback != null)
		{
			mCallback.LoggerCallback("D/", mTag, Message);
		}
	}
	
	public void warn(String Message)
	{
		if (bAllowLogging)
		{
			Log.w(mTag, Message);
		}
		if (mCallback != null)
		{
			mCallback.LoggerCallback("W/", mTag, Message);
		}
	}
	
	public void error(String Message)
	{
		if (bAllowLogging)
		{
			Log.e(mTag, Message);
		}
		if (mCallback != null)
		{
			mCallback.LoggerCallback("E/", mTag, Message);
		}
	}
}