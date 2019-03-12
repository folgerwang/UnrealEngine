//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.view.View;
import android.view.WindowManager;

import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

public class SplashActivity extends Activity
{
	private static final int PERMISSION_REQUEST_CODE = 1105;

	private Intent GameActivityIntent;
	private boolean WaitForPermission = false;


	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		boolean ShouldHideUI = false;
		boolean UseDisplayCutout = false;
		boolean IsShipping = false;
		boolean UseExternalFilesDir = false;
		try {
			ApplicationInfo ai = getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;

			if(bundle.containsKey("com.epicgames.ue4.GameActivity.bShouldHideUI"))
			{
				ShouldHideUI = bundle.getBoolean("com.epicgames.ue4.GameActivity.bShouldHideUI");
			}
			if(bundle.containsKey("com.epicgames.ue4.GameActivity.bUseDisplayCutout"))
			{
				UseDisplayCutout = bundle.getBoolean("com.epicgames.ue4.GameActivity.bUseDisplayCutout");
			}
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.BuildConfiguration"))
			{
				String Configuration = bundle.getString("com.epicgames.ue4.GameActivity.BuildConfiguration");
				IsShipping = Configuration.equals("Shipping");
			}
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bUseExternalFilesDir"))
            {
                UseExternalFilesDir = bundle.getBoolean("com.epicgames.ue4.GameActivity.bUseExternalFilesDir");
            }
		}
		catch (NameNotFoundException e)
		{
		}
		catch (NullPointerException e)
		{
		}

		if (ShouldHideUI)
		{ 
			View decorView = getWindow().getDecorView(); 
			// only do this on KitKat and above
			if(android.os.Build.VERSION.SDK_INT >= 19) {
				decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
											| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
											| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_FULLSCREEN
											| View.SYSTEM_UI_FLAG_IMMERSIVE);  // NOT sticky (will be set later in MainActivity)
			}
		}

		// for now only allow on one device manufacturer - fix up later
		if (!android.os.Build.MANUFACTURER.equals("HUAWEI"))
		{
			UseDisplayCutout = false;
		}

		if (UseDisplayCutout)
		{
			// only do this on Android Pie and above
			if (android.os.Build.VERSION.SDK_INT >= 28)
			{
	            WindowManager.LayoutParams params = getWindow().getAttributes();
		        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
			    getWindow().setAttributes(params);
			}
			else
			{
				UseDisplayCutout = false;
			}
		}

		GameActivityIntent = new Intent(this, GameActivity.class);
		GameActivityIntent.putExtras(getIntent());
		GameActivityIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		GameActivityIntent.putExtra("UseSplashScreen", "true");
		if (ShouldHideUI)
		{
			GameActivityIntent.putExtra("ShouldHideUI", "true");
		}
		if (UseDisplayCutout)
		{
			GameActivityIntent.putExtra("UseDisplayCutout", "true");
		}

		//pass down any extras added to this Activity's intent to the GameActivity intent (GCM data, for example)
		Intent intentFromActivity = getIntent();
		Bundle intentBundle = intentFromActivity.getExtras();
		if(intentBundle != null)
		{
			GameActivityIntent.putExtras(intentBundle);
		}
		
		// pass the action if available
		String intentAction = intentFromActivity.getAction();
		if (intentAction != null)
		{
			GameActivityIntent.setAction(intentAction);
		}

		// check if we need to wait for permissions
		int targetSdkVersion = 0;
		try 
		{
			PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
			targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;
		}
		catch (PackageManager.NameNotFoundException e) 
		{
		}

		if (android.os.Build.VERSION.SDK_INT >= 23 && targetSdkVersion >= 23) //23 is the API level (Marshmallow) where runtime permission handling is available
		{
			// we might need to ask for permission if we don't already have it
			if (ContextCompat.checkSelfPermission(this, "android.permission.WRITE_EXTERNAL_STORAGE") != PackageManager.PERMISSION_GRANTED)
			{
				if (!IsShipping || !UseExternalFilesDir)
				{
					ActivityCompat.requestPermissions(this, new String[] {"android.permission.WRITE_EXTERNAL_STORAGE"}, PERMISSION_REQUEST_CODE);
					WaitForPermission = true;
				}
			}
		}

		if (!WaitForPermission)
		{
			startActivity(GameActivityIntent);
			finish();
			overridePendingTransition(0, 0);
		}
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
	{
		if (requestCode==PERMISSION_REQUEST_CODE && permissions.length>0) 
		{
			if (grantResults.length>0 && grantResults[0]==PackageManager.PERMISSION_GRANTED)
			{
				startActivity(GameActivityIntent);
				finish();
				overridePendingTransition(0, 0);
			}
			else
			{
				finish();
			}
		}
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		if (!WaitForPermission)
		{
			finish();
			overridePendingTransition(0, 0);
		}
	}

}