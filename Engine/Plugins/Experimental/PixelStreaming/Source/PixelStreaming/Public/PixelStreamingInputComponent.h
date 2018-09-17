// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "PixelStreamingInputComponent.generated.h"

class IPixelStreamingPlugin;

/**
 * This component may be attached to an actor to allow UI interactions to be
 * handled as the delegate will be notified about the interaction and will be
 * supplied with a generic descriptor string containing, for example, JSON data.
 * Responses back to the source of the UI interactions may also be sent.
 */
UCLASS(Blueprintable, ClassGroup = (PixelStreaming), meta = (BlueprintSpawnableComponent))
class PIXELSTREAMING_API UPixelStreamingInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPixelStreamingInputComponent();
	
	// The delegate which will be notified about a UI interaction.
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPixelStreamingInput, const FString&, Descriptor);
	UPROPERTY(BlueprintAssignable, Category = "Pixel Streaming Input")
	FOnPixelStreamingInput OnPixelStreamingInputEvent;

	/**
	 * Run a built-in command. The commands are defined by a JSON descriptor.
	 * The currently supported commands are:
	 *
	 * 1. A command to run any console command:
	 *    "{ ConsoleCommand: <string> }"
	 *
	 * 2. A command to change the resolution to the given width and height.
	 *    "{ Resolution: { Width: <value>, Height: <value> } }"
	 *
	 * 3. A command to change the encoder settings by reducing the bitrate by the
	 *    given percentage.
	 *    "{ Encoder: { BitrateReduction: <value> } }
	 *
	 * @param Descriptor - The command JSON descriptor.
	 * @return True if the command was successfully executed.
	 */
	bool OnCommand(const FString& Descriptor);

	/**
	 * Send a response back to the source of the UI interactions.
	 * @param Descriptor - A generic descriptor string.
	 */
	UFUNCTION(BlueprintCallable, Category = "Pixel Streaming Input")
	void SendPixelStreamingResponse(const FString& Descriptor);

	UFUNCTION(BlueprintPure, Category = "Pixel Streaming Input", meta = (DeprecatedFunction, DeprecationMessage = "Use GetJsonStringValue instead"))
	bool GetJsonStringField(FString Descriptor, FString FieldName, FString& StringField);

	/**
	 * Helper function to extract a string field from a JSON descriptor of a
	 * UI interaction given its field name.
	 * The field name may be hierarchical, delimited by a period. For example,
	 * to access the Width value of a Resolution command above you should use
	 * "Resolution.Width" to get the width value.
	 * @param Descriptor - The UI interaction JSON descriptor.
	 * @param FieldName - The name of the field to look for in the JSON.
	 * @param StringValue - The string value associated with the field name.
	 * @param Success - True if the field exists in the JSON data.
	 */
	UFUNCTION(BlueprintPure, Category = "Pixel Streaming Input")
	void GetJsonStringValue(FString Descriptor, FString FieldName, FString& StringValue, bool& Success);

	/**
	 * Helper function to add a string field to a JSON descriptor. This produces
	 * a new descriptor which may then be chained to add further string fields.
	 * @param Descriptor - The initial JSON descriptor which may be blank initially.
	 * @param FieldName - The name of the field to add to the JSON.
	 * @param StringValue - The string value associated with the field name.
	 * @param NewDescriptor - The JSON descriptor with the string field added.
	 * @param Success - True if the string field could be added successfully.
	 */
	UFUNCTION(BlueprintPure, Category = "Pixel Streaming Input")
	void AddJsonStringValue(const FString& Descriptor, FString FieldName, FString StringValue, FString& NewDescriptor, bool& Success);

private:

	// For convenience we keep a reference to the Pixel Streaming plugin.
	IPixelStreamingPlugin* PixelStreamingPlugin;
};
