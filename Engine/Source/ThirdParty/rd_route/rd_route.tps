<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>rd_route v1.1.1</Name>
  <Location>/Engine/Source/ThirdParty/rd_route/</Location>
  <Function>Provides us with the ability to 'hook' or 'interpose' system functions with our one implementation. Normally this is not required but because we want to replace the global allocator with our own there is one system function in Apple's CFNetwork library that we must replace without our implementation or the engine will crash.</Function>
  <Eula>https://github.com/rodionovd/rd_route/blob/master/LICENSE</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>None</LicenseFolder>
</TpsData>