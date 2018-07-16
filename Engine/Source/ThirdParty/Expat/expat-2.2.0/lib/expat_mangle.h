#ifndef EXPAT_MANGLE_H
#define EXPAT_MANGLE_H

/* Support mangling the expat functions */
#define XML_ParserCreate(encodingName) UE4_XML_ParserCreate(encodingName) 
#define XML_ParserCreateNS(encodingName, nsSep) UE4_XML_ParserCreateNS(encodingName, nsSep) 
#define XML_ParserCreate_MM(encoding, memsuite, namespaceSeparator) UE4_XML_ParserCreate_MM(encoding, memsuite, namespaceSeparator) 
#define XML_ExternalEntityParserCreate(parser, context, encoding) UE4_XML_ExternalEntityParserCreate(parser, context, encoding) 
#define XML_ParserFree(parser) UE4_XML_ParserFree(parser) 
#define XML_Parse(parser, s, len, isFinal) UE4_XML_Parse(parser, s, len, isFinal) 
#define XML_ParseBuffer(parser, len, isFinal) UE4_XML_ParseBuffer(parser, len, isFinal) 
#define XML_GetBuffer(parser, len) UE4_XML_GetBuffer(parser, len) 
#define XML_SetStartElementHandler(parser, start) UE4_XML_SetStartElementHandler(parser, start) 
#define XML_SetEndElementHandler(parser, end) UE4_XML_SetEndElementHandler(parser, end) 
#define XML_SetElementHandler(parser, start, end) UE4_XML_SetElementHandler(parser, start, end) 
#define XML_SetCharacterDataHandler(parser, handler) UE4_XML_SetCharacterDataHandler(parser, handler) 
#define XML_SetProcessingInstructionHandler(parser, handler) UE4_XML_SetProcessingInstructionHandler(parser, handler) 
#define XML_SetCommentHandler(parser, handler) UE4_XML_SetCommentHandler(parser, handler) 
#define XML_SetStartCdataSectionHandler(parser, start) UE4_XML_SetStartCdataSectionHandler(parser, start) 
#define XML_SetEndCdataSectionHandler(parser, end) UE4_XML_SetEndCdataSectionHandler(parser, end) 
#define XML_SetCdataSectionHandler(parser, start, end) UE4_XML_SetCdataSectionHandler(parser, start, end) 
#define XML_SetDefaultHandler(parser, handler) UE4_XML_SetDefaultHandler(parser, handler) 
#define XML_SetDefaultHandlerExpand(parser, handler) UE4_XML_SetDefaultHandlerExpand(parser, handler) 
#define XML_SetExternalEntityRefHandler(parser, handler) UE4_XML_SetExternalEntityRefHandler(parser, handler) 
#define XML_SetExternalEntityRefHandlerArg(parser, arg) UE4_XML_SetExternalEntityRefHandlerArg(parser, arg) 
#define XML_SetUnknownEncodingHandler(parser, handler, data) UE4_XML_SetUnknownEncodingHandler(parser, handler, data) 
#define XML_SetStartNamespaceDeclHandler(parser, start) UE4_XML_SetStartNamespaceDeclHandler(parser, start) 
#define XML_SetEndNamespaceDeclHandler(parser, end) UE4_XML_SetEndNamespaceDeclHandler(parser, end) 
#define XML_SetNamespaceDeclHandler(parser, start, end) UE4_XML_SetNamespaceDeclHandler(parser, start, end) 
#define XML_SetXmlDeclHandler(parser, handler) UE4_XML_SetXmlDeclHandler(parser, handler) 
#define XML_SetStartDoctypeDeclHandler(parser, start) UE4_XML_SetStartDoctypeDeclHandler(parser, start) 
#define XML_SetEndDoctypeDeclHandler(parser, end) UE4_XML_SetEndDoctypeDeclHandler(parser, end) 
#define XML_SetDoctypeDeclHandler(parser, start, end) UE4_XML_SetDoctypeDeclHandler(parser, start, end) 
#define XML_SetElementDeclHandler(parser, eldecl) UE4_XML_SetElementDeclHandler(parser, eldecl) 
#define XML_SetAttlistDeclHandler(parser, attdecl) UE4_XML_SetAttlistDeclHandler(parser, attdecl) 
#define XML_SetEntityDeclHandler(parser, handler) UE4_XML_SetEntityDeclHandler(parser, handler) 
#define XML_SetUnparsedEntityDeclHandler(parser, handler) UE4_XML_SetUnparsedEntityDeclHandler(parser, handler) 
#define XML_SetNotationDeclHandler(parser, handler) UE4_XML_SetNotationDeclHandler(parser, handler) 
#define XML_SetNotStandaloneHandler(parser, handler) UE4_XML_SetNotStandaloneHandler(parser, handler) 
#define XML_GetErrorCode(parser) UE4_XML_GetErrorCode(parser) 
#define XML_ErrorString(code) UE4_XML_ErrorString(code) 
#define XML_GetCurrentByteIndex(parser) UE4_XML_GetCurrentByteIndex(parser) 
#define XML_GetCurrentLineNumber(parser) UE4_XML_GetCurrentLineNumber(parser) 
#define XML_GetCurrentColumnNumber(parser) UE4_XML_GetCurrentColumnNumber(parser) 
#define XML_GetCurrentByteCount(parser) UE4_XML_GetCurrentByteCount(parser) 
#define XML_GetInputContext(parser, offset, size) UE4_XML_GetInputContext(parser, offset, size) 
#define XML_SetUserData(parser, userData) UE4_XML_SetUserData(parser, userData) 
#define XML_DefaultCurrent(parser) UE4_XML_DefaultCurrent(parser) 
#define XML_UseParserAsHandlerArg(parser) UE4_XML_UseParserAsHandlerArg(parser) 
#define XML_SetBase(parser, base) UE4_XML_SetBase(parser, base) 
#define XML_GetBase(parser) UE4_XML_GetBase(parser) 
#define XML_GetSpecifiedAttributeCount(parser) UE4_XML_GetSpecifiedAttributeCount(parser) 
#define XML_GetIdAttributeIndex(parser) UE4_XML_GetIdAttributeIndex(parser) 
#define XML_SetEncoding(parser, encoding) UE4_XML_SetEncoding(parser, encoding) 
#define XML_SetParamEntityParsing(parser, parsing) UE4_XML_SetParamEntityParsing(parser, parsing) 
#define XML_SetReturnNSTriplet(parser, do_nst) UE4_XML_SetReturnNSTriplet(parser, do_nst) 
#define XML_ExpatVersion(void) UE4_XML_ExpatVersion(void) 
#define XML_ExpatVersionInfo(void) UE4_XML_ExpatVersionInfo(void) 
#define XML_ParserReset(parser, encoding) UE4_XML_ParserReset(parser, encoding) 
#define XML_SetSkippedEntityHandler(parser, handler) UE4_XML_SetSkippedEntityHandler(parser, handler) 
#define XML_UseForeignDTD(parser, useDTD) UE4_XML_UseForeignDTD(parser, useDTD) 
#define XML_GetFeatureList(void) UE4_XML_GetFeatureList(void) 
#define XML_StopParser(parser, resumable) UE4_XML_StopParser(parser, resumable) 
#define XML_ResumeParser(parser) UE4_XML_ResumeParser(parser) 
#define XML_GetParsingStatus(parser, status) UE4_XML_GetParsingStatus(parser, status) 
#define XML_FreeContentModel(parser, model) UE4_XML_FreeContentModel(parser, model) 
#define XML_MemMalloc(parser, size) UE4_XML_MemMalloc(parser, size) 
#define XML_MemRealloc(parser, ptr, size) UE4_XML_MemRealloc(parser, ptr, size) 
#define XML_MemFree(parser, ptr) UE4_XML_MemFree(parser, ptr) 
#define XML_SetHashSalt(parser, hashSalt) UE4_XML_SetHashSalt(parser, hashSalt)

#define XmlPrologStateInit UE4_XmlPrologStateInit
#define XmlPrologStateInitExternalEntity UE4_XmlPrologStateInitExternalEntity
#define XmlGetUtf16InternalEncoding UE4_XmlGetUtf16InternalEncoding
#define XmlGetUtf16InternalEncodingNS UE4_XmlGetUtf16InternalEncodingNS
#define XmlGetUtf8InternalEncoding UE4_XmlGetUtf8InternalEncoding
#define XmlGetUtf8InternalEncodingNS UE4_XmlGetUtf8InternalEncodingNS
#define XmlInitEncoding UE4_XmlInitEncoding
#define XmlInitEncodingNS UE4_XmlInitEncodingNS
#define XmlInitUnknownEncoding UE4_XmlInitUnknownEncoding
#define XmlInitUnknownEncodingNS UE4_XmlInitUnknownEncodingNS
#define XmlParseXmlDecl UE4_XmlParseXmlDecl
#define XmlParseXmlDeclNS UE4_XmlParseXmlDeclNS
#define XmlSizeOfUnknownEncoding UE4_XmlSizeOfUnknownEncoding
#define XmlUtf16Encode UE4_XmlUtf16Encode
#define XmlUtf8Encode UE4_XmlUtf8Encode

#endif /* EXPAT_MANGLE_H */
