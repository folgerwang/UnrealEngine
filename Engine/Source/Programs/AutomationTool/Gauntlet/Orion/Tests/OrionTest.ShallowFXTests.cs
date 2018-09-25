// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.IO;
using System.Diagnostics;
using Gauntlet;
using Newtonsoft.Json;

namespace OrionTest
{
    public class ShallowFXTests : TestEngS3Base
    {
        string BuildName;
        bool bShouldUseBackupEmailer;
        public ShallowFXTests(UnrealTestContext InContext)
            : base(InContext)
        {

        }
        public class FXFormattedPerfAbilityDataResult
        {
            public string MapName;
            public string TestName;
            public List<string> RelevantStats;
            public Dictionary<string, float> AvgValues;
            public Dictionary<string, float> MaxValues;
            public List<string> YellowAvgs;
            public List<string> YellowMaxes;
            public List<string> RedAvgs;
            public List<string> RedMaxes;
            public FXFormattedPerfAbilityDataResult()
            {
                MapName = "";
                TestName = "";
                AvgValues = new Dictionary<string, float>();
                MaxValues = new Dictionary<string, float>();
                YellowAvgs = new List<string>();
                YellowMaxes = new List<string>();
                RedAvgs = new List<string>();
                RedMaxes = new List<string>();
                RelevantStats = new List<string>();
            }
        }
        public class FXPerfThreshold
        {
            public string statName;
            public float avgYellow;
            public float maxYellow;
            public float avgRed;
            public float maxRed;
            public FXPerfThreshold()
            { }
        }
        public class FXPerfThresholdCollection
        {
            public List<FXPerfThreshold> thresholds;
            public string mapName;
            public FXPerfThresholdCollection()
            { }
        }

        // Need this to get rid of the occasional warnings due to unused json blob values. :/ 
        public void ClearOutUnnecessaryFields(FXPerfDataEntry EntryToClear)
        {
            EntryToClear.appId = "";
            EntryToClear.appVersion = "";
            EntryToClear.userId = "";
            EntryToClear.ipAddress = "";
            EntryToClear.timeOfGenerationUTC = "";
            EntryToClear.deviceProfile = "";
            EntryToClear.platform = "";
            EntryToClear.currentDate = "";
            EntryToClear.testGuid = "";
        }
        public class FXPerfDataEntry
        {
            public string appId;
            public string appVersion;
            public string userId;
            public string ipAddress;
            public string timeOfGenerationUTC;
            public string statName;
            public string mapName;
            public string testName;
            public string deviceProfile;
            public string platform;
            public float minVal;
            public float avgVal;
            public float maxVal;
            public int numFrames;
            public int cL;
            public string currentDate;
            public string testGuid;
            public int iterationNumber;
            public FXPerfDataEntry()
            {
            }
        }

        List<FXPerfThresholdCollection> Thresholds;
        List<FXPerfDataEntry> DataEntries;

        public void GenerateDataFromOutputFile(string PathToFile)
        {
            Console.WriteLine("Looking at file: " + PathToFile);
            List<FXFormattedPerfAbilityDataResult> FormattedResults = new List<FXFormattedPerfAbilityDataResult>();
            FXFormattedPerfAbilityDataResult FormattedResult = new FXFormattedPerfAbilityDataResult();
           
            if (File.Exists(PathToFile))
            {
                StreamReader TextInFile = new StreamReader(PathToFile, Encoding.Unicode);
                while (!TextInFile.EndOfStream)
                {
                    try
                    {
                        string LineToParse = TextInFile.ReadLine();
                        Console.WriteLine(LineToParse);
                        FXPerfDataEntry DataEntry = JsonConvert.DeserializeObject<FXPerfDataEntry>(LineToParse);
                        DataEntries.Add(DataEntry);
                    }
                    catch (Exception e)
                    {
                        Console.WriteLine("Error deserializing FXPerf Data Entry");
                        Console.WriteLine(e.Message);
                    }
                }
                TextInFile.Close();
            }
            string RemoteThresholdLocation = "\\\\epicgames.net\\Root\\Builds\\Orion\\Automation\\Config\\OrionPerfThresholds.json";
            if (File.Exists(RemoteThresholdLocation))
            {
                StreamReader ThresholdReader = new StreamReader(RemoteThresholdLocation);
                try
                {
                    Thresholds = JsonConvert.DeserializeObject<List<FXPerfThresholdCollection>>(ThresholdReader.ReadToEnd());
                    Dictionary<string, List<FXPerfThreshold>> MapThresholds = new Dictionary<string, List<FXPerfThreshold>>();
                    for (int i = 0; i < Thresholds.Count; i++)
                    {
                        if (!MapThresholds.ContainsKey(Thresholds[i].mapName))
                        {
                            MapThresholds.Add(Thresholds[i].mapName, Thresholds[i].thresholds);
                        }
                    }

                    for (int i = 0; i < DataEntries.Count; i++)
                    {
                        // We ONLY want iteration 2 numbers.
                        if (DataEntries[i].iterationNumber != 2)
                        {
                            continue;
                        }

                        if (DataEntries[i].mapName != FormattedResult.MapName || DataEntries[i].testName != FormattedResult.TestName)
                        {
                            if (FormattedResult.MapName != "" || FormattedResult.TestName != "")
                            {
                                FormattedResults.Add(FormattedResult);
                            }
                            FormattedResult = new FXFormattedPerfAbilityDataResult();
                            FormattedResult.MapName = DataEntries[i].mapName;
                            FormattedResult.TestName = DataEntries[i].testName;
                        }

                        FormattedResult.MaxValues.Add(DataEntries[i].statName, DataEntries[i].maxVal);
                        FormattedResult.AvgValues.Add(DataEntries[i].statName, DataEntries[i].avgVal);

                        // If we have a threshold set for this map, we want to use it where possible. Otherwise just stick with the default.
                        List<FXPerfThreshold> ThresholdsToUse = MapThresholds["Default"];
                        if (MapThresholds.ContainsKey(DataEntries[i].mapName))
                        {
                            ThresholdsToUse = MapThresholds[DataEntries[i].mapName];
                        }

                        for (int j = 0; j < ThresholdsToUse.Count; j++)
                        {
                            if (ThresholdsToUse[j].statName != DataEntries[i].statName)
                            {
                                continue;
                            }
                            string TestDetailString = String.Format("{0}-{1} {2}:", DataEntries[i].mapName, DataEntries[i].testName, DataEntries[i].statName);
                            TestDetailString = TestDetailString.Replace("FTEST_FXShallow_", "").Replace("STAT_", "");
                            if (DataEntries[i].avgVal > ThresholdsToUse[j].avgRed)
                            {
                                FormattedResult.RedAvgs.Add(DataEntries[i].statName);
                            }
                            else if (DataEntries[i].avgVal > ThresholdsToUse[j].avgYellow)
                            {
                                FormattedResult.YellowAvgs.Add(DataEntries[i].statName);
                            }

                            if (DataEntries[i].maxVal > ThresholdsToUse[j].maxRed)
                            {
                                FormattedResult.RedMaxes.Add(DataEntries[i].statName);
                            }
                            else if (DataEntries[i].maxVal > ThresholdsToUse[j].maxYellow)
                            {
                                FormattedResult.YellowMaxes.Add(DataEntries[i].statName);
                            }

                            // We generate a list of stats we care about for each map/test here so we can dynamically print stats we care about test over test.
                            FormattedResult.RelevantStats.Add(DataEntries[i].statName);

                        }
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine("Failed to deserialize Threshold Json");
                    Console.WriteLine(e.Message);
                }
                ThresholdReader.Close();
                FormattedResults.Add(FormattedResult);
            }
            else
            {
                Console.WriteLine("Failed to find Threshold File at " + RemoteThresholdLocation + " - perf data is saved in datarouter but no report generated.");
            }
            List<FXFormattedPerfAbilityDataResult> RedResults = new List<FXFormattedPerfAbilityDataResult>();
            List<FXFormattedPerfAbilityDataResult> YellowResults = new List<FXFormattedPerfAbilityDataResult>();

            // We want to bubble up reports into Red and Yellow subsections. Prepping lists like this makes report generation far tidier.
            for (int i = 0; i < FormattedResults.Count; i++)
            {
                if (FormattedResults[i].RedMaxes.Count > 0 || FormattedResults[i].RedAvgs.Count > 0)
                {
                    RedResults.Add(FormattedResults[i]);
                }
                else if (FormattedResults[i].YellowMaxes.Count > 0 || FormattedResults[i].YellowAvgs.Count > 0)
                {
                    YellowResults.Add(FormattedResults[i]);
                }
            }

            string DataString = "<font size=+2>Shallow FX Performance Report for " + Context.BuildInfo.Name + " " + Context.ClientConfiguration + "</font><br>";
            if (RedResults.Count > 0)
            {
                DataString += "<font size=+2><b>Red Tests</b></font><br>";
                for (int i = 0; i < RedResults.Count; i++)
                {
                    string TestDetailString = String.Format("{0} - {1}:", RedResults[i].MapName.Replace("FTEST_FXShallow_", ""), RedResults[i].TestName);
                    DataString += "<b>" + TestDetailString + "</b><br>";
                    DataString += CreateTableFromFormattedResult(RedResults[i]);
                }
            }

            if (YellowResults.Count > 0)
            {
                DataString += "<font size=+2><b>Yellow Tests</b></font><br>";
                for (int i = 0; i < YellowResults.Count; i++)
                {
                    string TestDetailString = String.Format("{0} - {1}:", YellowResults[i].MapName.Replace("FTEST_FXShallow_", ""), YellowResults[i].TestName);
                    DataString += "<b>" + TestDetailString + "</b><br>";
                    DataString += CreateTableFromFormattedResult(YellowResults[i]);
                }
            }

            DataString += "<font size=+2><b>All Tests</b></font><br>";
            for (int i = 0; i < FormattedResults.Count; i++)
            {
                string TestDetailString = String.Format("{0} - {1}:", FormattedResults[i].MapName.Replace("FTEST_FXShallow_", ""), FormattedResults[i].TestName);
                DataString += "<b>" + TestDetailString + "</b><br>";
                DataString += CreateTableFromFormattedResult(FormattedResults[i]);
            }

            // Emails will only work on build machines. Save report as HTML otherwise. 
            OrionReportMailer Mailer = new OrionReportMailer();
            string MessageSubject = "Shallow FX perf report for " + Context.BuildInfo.Name;
            if (Mailer.SendReportEmail("FXPerfReports", MessageSubject, DataString) == false)
            {
                if (!bShouldUseBackupEmailer || Mailer.SendReportEmailBackup("FXPerfReports", MessageSubject, DataString) == false)
                {
                    Console.WriteLine("Failed to send email, so saving mail as .html file instead!");
                    Console.WriteLine("Writing file to " + PathToFile);
                    StreamWriter htmlWriter = new StreamWriter(PathToFile + ".html");
                    htmlWriter.Write(DataString);
                    htmlWriter.Close();
                }
            }
        }


        public override UnrealTestConfiguration GetConfiguration()
        {
            UnrealTestConfiguration Config = base.GetConfiguration();

            // save build name
            BuildName = Context.BuildInfo.Name;

            Config.ServerCommandline = "-notimeouts -unattended -nosbmm ";
            string MapName = Context.TestParams.ParseValue("hero", "");
            string[] HeroesToUse = MapName.Split(',');
            if (HeroesToUse.Length > 0)
            {
                MapName = "OrionPerf.EffectsShallow.Heroes." + HeroesToUse[0];
                for (int i = 1; i < HeroesToUse.Length; i++)
                {
                    MapName += "+OrionPerf.EffectsShallow.Heroes." + HeroesToUse[i];
                }
            }
            else
            {
                MapName = "OrionPerf.EffectsShallow.Heroes." + MapName;
            }
            string AutomationTestCommandLine = String.Format("-AutoJoinTutorialLevelCap=0 -ExecCmds=\"Automation RunTests {0};quit\" ", MapName);
            if (Context.TestParams.ParseParam("testing"))
            {
                AutomationTestCommandLine += "-nullrhi";
            }
            if (Context.TestParams.ParseParam("localmailer"))
            {
                bShouldUseBackupEmailer = true;
            }
            else
            {
                bShouldUseBackupEmailer = false;
            }


            Config.ClientCommandline = AutomationTestCommandLine;
            Config.MaxDuration = 360 * 60;
			Config.ScreenshotPeriod = 0;
            return Config;

        }

        public string CreateTableFromFormattedResult(FXFormattedPerfAbilityDataResult InResult)
        {
            // Open up the first row with an empty cell since no stat name will go in there.
            string FormattedString = "<table border=\"1px solid black\"><tr><td></td>";

            foreach (string KeyName in InResult.AvgValues.Keys)
            {
                if (InResult.RelevantStats.Contains(KeyName))
                {
                    FormattedString += "<td>" + KeyName + "</td>";
                }
            }
            FormattedString += "</tr><tr><td>Avg Val</td>";
            foreach (string KeyName in InResult.AvgValues.Keys)
            {
                string BGColorToUse = "white";
                if (InResult.RelevantStats.Contains(KeyName))
                {
                    if (InResult.YellowAvgs.Contains(KeyName))
                    {
                        BGColorToUse = "yellow";
                    }
                    else if (InResult.RedAvgs.Contains(KeyName))
                    {
                        BGColorToUse = "red";
                    }
                    FormattedString += "<td bgcolor=\""+ BGColorToUse +"\">" + InResult.AvgValues[KeyName].ToString() + "</td>";
                }
            }
            FormattedString += "</tr><tr><td>Max Val</td>";
            foreach (string KeyName in InResult.MaxValues.Keys)
            {
                string BGColorToUse = "white";
                if (InResult.RelevantStats.Contains(KeyName))
                {
                    if (InResult.YellowMaxes.Contains(KeyName))
                    {
                        BGColorToUse = "yellow";
                    }
                    else if (InResult.RedMaxes.Contains(KeyName))
                    {
                        BGColorToUse = "red";
                    }
                    FormattedString += "<td bgcolor=\"" + BGColorToUse + "\">" + InResult.MaxValues[KeyName].ToString() + "</td>";
                }
            }
            FormattedString += "</tr></table>";
            return FormattedString;
        }

        public override void SaveArtifacts(string OutputPath)
        {
			base.SaveArtifacts(OutputPath);

			UnrealLogParser Parser = new UnrealLogParser(TestInstance.ServerApp.Output);
            DataEntries = new List<FXPerfDataEntry>();
            Thresholds = new List<FXPerfThresholdCollection>();
            string[] StartedLines = Parser.GetAllMatchingLines("FXTests*");
            string S3LogPath = "";


            string ArtifactDir = TestInstance.ClientApps[0].GetArtifactPath();

            string BaseLogDir = Path.Combine(ArtifactDir, "fxperformance");
            if (Directory.Exists(BaseLogDir))
            {
                foreach (string FileName in Directory.GetFiles(BaseLogDir, "*", SearchOption.AllDirectories))
                {
                    if (FileName.Contains("guid="))
                    {
                        GenerateDataFromOutputFile(FileName);
                    }

                }
                RecursiveCopyToS3(BaseLogDir.ToLower(), S3LogPath, true);
            }
            else
            {
                Log.Info(String.Format("Base log directory not found at {0}", BaseLogDir));
            }
            foreach (string Line in StartedLines)
            {
                Log.Info(Line);
            }
        }

        protected override TestResult GetCompletedResult()
        {
            int Result = 0;
            
            if (TestInstance.ClientApps == null)
            {
                // If no client we can just use the server exit code
                Result = FindExitCauseAndGetReturnCode("Server", TestInstance.ServerApp);
            }
            else
            {
                // If we have clients and the server exited, something went wrong...
                if (TestInstance.ServerApp != null && TestInstance.ServerApp.HasExited)
                {
                    Log.Info("Server exited unexpectedly! Ending test");
                    FindExitCauseAndGetReturnCode("Server", TestInstance.ServerApp);
                    Result = -1;
                }
                else
                {
                    for (int i = 0; i < TestInstance.ClientApps.Length; i++)
                    {
                        // Give the system a couple of seconds to clean up the checkpoint file upon test pass completion.
                        System.Threading.Thread.Sleep(2000);
                        string ArtifactDir = TestInstance.ClientApps[i].GetArtifactPath();
                        if (File.Exists(ArtifactDir + "\\automation\\automationcheckpoint.txt"))
                        {
                            Log.Info("Checkpoint file found! Restarting pass and continuing.");
                            return TestResult.WantRetry;
                        }
                        int Code = FindExitCauseAndGetReturnCode(string.Format("Client{0}", i), TestInstance.ClientApps[i]);

                        // We only return one result, so return one of the non-zero values
                        if (Code != 0)
                        {
                            Result = Code;
                        }
                    }
                }
            }

            return Result == 0 ? TestResult.Passed : TestResult.Failed;
        }
        public static int ExecuteCommand(string command, int timeout)
        {
            var processInfo = new ProcessStartInfo("cmd.exe", "/C " + command)
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                WorkingDirectory = "C:\\",
            };

            var process = Process.Start(processInfo);
            process.WaitForExit(timeout);
            var exitCode = process.ExitCode;
            process.Close();
            return exitCode;
        }
    }
}
