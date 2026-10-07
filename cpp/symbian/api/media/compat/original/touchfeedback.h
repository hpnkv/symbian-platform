/*
* Copyright (c) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:  Interface for using area registry and direct feedback from
*                applications and UI controls.
* Part of:      Tactile Feedback.
*
*/



#ifndef M_TOUCHFEEDBACK_H
#define M_TOUCHFEEDBACK_H

#include <e32std.h>
#include <e32base.h>
#include <coemain.h>

#include <touchlogicalfeedback.h>
#include <touchfeedbackspec.h>

class CCoeControl;

/**
 *  This is the Tactile Feedback interface for UI Controls.
 *
 *  Provides an interface to add, modify and remove feedback areas
 *  in the registry. There is also an option to trigger direct feedback,
 *  hence bypassing the registry.
 *
 *  Feedback areas must always be related to some UI Control (derived
 *  from CCoeControl). Areas are distinguished from each other based on
 *  control's address and an index number (so that it is possible
 *  to register and maintain multiple areas for same control).
 *
 *  Clients must add, maintain and remove their feedback areas using this
 *  API according to the state changes of the application / control, and
 *  according to for e.g. device wide layout changes.
 *
 *  There are two cases when tactile framework automatically updates the
 *  feedback areas for control: Feedback is always disabled when control
 *  becomes dimmed and re-enabled when control becomes undimmed again.
 *  If control becomes invisible, then feedback areas are (temporarily)
 *  removed, and they will be automatically added again when control
 *  becomes visible again.
 *
 *  This class is not intended for derivation outside the library.
 *
 *  @lib touchfeedback.lib
 *  @since S60 v5.0
 */
class MTouchFeedback
    {
public:

    /**
     * Used for acquiring a pointer to touch feedback instance.
     *
     * Pointer is retrieved from thread local storage, and thus it is best
     * to store the returned pointer as member variable in case it will
     * be needed frequently.
     *
     * NULL is returned in case there is no instance. In that case
     * CreateInstanceL -function can be used for creating a new instance.
     *
     * @since S60 5.0
     * @return Pointer to touch feedback instance created for this
     *         application process.
     */
    IMPORT_C static MTouchFeedback* Instance();

    /**
     * Creates a new touch feedback instance. The usage of
     * this function should only be necessary from processes which
     * are not GUI applications, but which still have user interface
     * (or want to play direct feedback).
     *
     * DestroyInstance -function must be called for deleting the instance
     * when it is no longer needed.
     *
     * @since S60 5.0
     * @return Pointer to new touch feedback instance.
     */
    IMPORT_C static MTouchFeedback* CreateInstanceL();

    /**
     * Destroys the touch feedback instance and clears pointer in
     * thread local storage.
     *
     * This function must only be used in case touch feedback has been
     * created with CreateInstanceL -function. I.e. normal GUI applications
     * should never call this function.
     *
     * @since S60 5.0
     */
    IMPORT_C static void DestroyInstance();

    /**
     * This function can be used to check, whether touch feedback is
     * supported at all in the device.
     *
     * All the API functions can be called safely even if touch feedback
     * is not enabled (for e.g. in devices without touch screen). But in
     * some situations registry updates can require complex calculations,
     * which can be skipped if touch feedback is not enabled at all.
     *
     * Notice that the settings related to touch feedback have no effect
     * on the behavior of this function. I.e. even if user turns touch
     * feedback OFF from settings, this function still returns
     * ETrue. The reason for this is that registry updates must be done
     * anyway even if the feedback is not on for the moment, because
     * user can turn it on at anytime, and it is not possible to force
     * an update for all applications in that case.
     *
     * @since S60 5.0
     * @return ETrue if touch feedback is supported in this device.
     */
    virtual TBool TouchFeedbackSupported() = 0;

    /**
     * Used for disabling or enabling feedback in the application.
     *
     * Tactile feedback is enabled by default, and thus standard
     * S60 components (such as CBA, lists and options menu) automatically
     * give feedback even if the application itself would make no effort
     * for producing feedback.
     *
     * For some applications (such as games) feedback might not be
     * wanted at all. In addition some applications may need to disable
     * feedback in some specific situations. For example: A camera
     * application may need to disable feedback during video recording,
     * because otherwise feedbacks may cause disturbing sounds that
     * will be recorded into the video clip.
     *
     * Notice that this function only affects direct feedback and
     * area registry based feedback for this application. I.e. if this
     * application is taken to background, other applications can still
     * produce feedback.
     *
     * Also notice that enabling feedback doesn't still mean that feedback
     * would necessarily be generated, because user may have disabled the
     * feedback for whole device from the settings.
     *
     * @since S60 5.0
     * @param aEnabled - Give ETrue as parameter for enabling feedback, and
     *                   EFalse for disabling feedback.
     */
    virtual void SetFeedbackEnabledForThisApp( TBool aEnabled ) = 0;

    /**
     * Used to check whether feedback is enabled for this application.
     *
     * Notice that this function only returns what was given as parameter
     * to SetFeedbackEnabledForThisApp -function. I.e. this function
     * can return ETrue even if  feedback would be currently disabled
     * from settings.
     *
     * If only vibra or audio feedback is enabled, this function still
     * returns ETrue.
     *
     * @since S60 5.0
     * @return ETrue if feedback is enabled for this application.
     */
    virtual TBool FeedbackEnabledForThisApp() = 0;

    /**
     * Sets or updates rectangular feedback area to registry.
     *
     * If this is new area (i.e. there is not yet area with given control
     * address and area index in the registry), then this area will be
     * added as the top priority area for its window, i.e. this
     * area will be hit test first when pointer event arrives.
     *
     * Notice however, that this area will remain as top priority area only
     * until the next new area is added to the registry, or until
     * MoveFeedbackAreaToFirstPriority -function is called. I.e. new areas
     * are always put on top priority, but they will only remain on top
     * until they will be overridden by next area.
     *
     * The control that is given as parameter should usually be the one
     * that is responsible of handling the pointer events on the
     * corresponding feedback area.
     *
     * The area can later be identified by passing control pointer and
     * index as parameters to the other functions (for modifying or
     * removing the feedback area). In case control only
     * registers one area, then index 0 can always be used. Usually most
     * sensible approach is to use indexes 1, 2, 3 etc. for additional
     * feedback areas, but in practice any desired index values can be
     * used.
     *
     * Notice that if given control is dimmed, then feedback type will
     * be set to "None". If given control is not visible, then feedback
     * area will not be added to registry at all (for now). However,
     * registry is automatically updated when control's dimming or
     * visibility changes, so one can call this function also for
     * dimmed and invisible control.
     *
     * When the control given as parameter to this function is destroyed,
     * then the RemoveFeedbackForControl -function must be called while
     * giving the same control as parameter again. This is necessary
     * for removing all the feedback areas, and also for resetting the
     * state information stored by the API implementation.
     *
     * @since S60 5.0
     * @param aControl      - The control handling pointer events on this
                              feedback area.
     * @param aIndex        - The index number of the area to be added.
     * @param aRect         - The feedback area rectangle.
     * @param aFeedbackType - The logical feedback type given
     * @param aEventType    - The pointer event type that triggers the
     *                        feedback (currently only ETouchEventStylusDown
     *                        is supported).
     * @return KErrNone, or one of standard Symbian OS error codes
     *         if setting of area to registry failed.
     *         Some specific error codes:
     *         KErrArgument - A NULL pointer was given as first parameter, or
     *                        the given control does not have any window
     *                        associated to it.
     *         KErrNotSupported - Unsupported logical feedback type or
     *                            event type was given as parameter.
     */
    virtual TInt SetFeedbackArea( const CCoeControl* aControl,
                                  TUint32 aIndex,
                                  TRect aRect,
                                  TTouchLogicalFeedback aFeedbackType,
                                  TTouchEventType aEventType ) = 0;

    /**
     * Removes feedback area from the registry.
     *
     * This function is designed to be used in case feedback areas
     * need to be removed elsewhere than in control's destructor. In
     * control's destructor RemoveFeedbackForControl -function must be
     * used instead.
     *
     * @since S60 5.0
     * @param aControl - The control, who has registered the area.
     * @param aIndex   - The index of the area to be removed.
     */
    virtual void RemoveFeedbackArea( const CCoeControl* aControl,
                                     TUint32 aIndex ) = 0;

    /**
     * Removes all feedback areas of specified control from registry.
     *
     * This function also clears all related data that has been cached
     * by the API implementation, and thus it must always be called from
     * control's destructor in case control has used any of the following
     * functions:
     *   - SetFeedbackArea
     *   - EnableFeedbackForControl
     *
     * Especially notice that it is not enough to remove all feedback areas
     * individually by using RemoveFeedbackArea -function.
     *
     * The difference between this function and EnableFeedbackForControl
     * -function is that this function really removes all areas related
     * to this control from registry, whereas EnableFeedbackForControl
     * (when EFalse if given as parameter) only temporarily disables
     * those areas.
     *
     * @since S60 5.0
     * @param aControl - Pointer to the control, whose area registry
     *                   entries and cached information will be removed.
     */
    virtual void RemoveFeedbackForControl( const CCoeControl* aControl ) = 0;

    /**
     * Changes feedback area in the registry.
     *
     * The area must have been originally added to the registry with
     * SetFeedbackArea -function, or otherwise this function will
     * do nothing.
     *
     * This function is intended to be used especially in
     * portrait / landscape changes.
     *
     * If given CCoeControl pointer is NULL, then this function call
     * does nothing.
     *
     * @since S60 5.0
     * @param aControl - The control, who has registered the area.
     * @param aIndex   - The index number of the area, that will be changed.
     * @param aNewRect - New area rectangle for given feedback area.
     */
    virtual void ChangeFeedbackArea( const CCoeControl* aControl,
                                     TUint32 aIndex,
                                     TRect aNewRect ) = 0;

    /**
     * Changes feedback type in the registry.
     *
     * Feedback can be temporarily disabled by giving ETouchFeedbackNone
     * as parameter (although using EnableFeedbackForControl function with
     * parameter EFalse is usually better for this functionality).
     *
     * The area must have been originally added to the registry with
     * SetFeedbackArea -function, or otherwise this function will
     * do nothing.
     *
     * If given control pointer is NULL, then this function call
     * is ignored.
     *
     * This function only changes feedback type for down event.
     * To change feedback types for all the events in the feedback area use
     * function SetFeedbackArea with CFeedbackSpec container class
     * definion instead.
     *
     * @since S60 5.0
     * @param aControl - The control, who has registered the area.
     * @param aIndex   - The index number of the area, that will be changed.
     * @param aNewType - New feedback type for the area.
     */
    virtual void ChangeFeedbackType( const CCoeControl* aControl,
                                     TUint32 aIndex,
                                     TTouchLogicalFeedback aNewType ) = 0;


    /**
     * Makes the given feedback area the first priority area in the window
     * where it is located.
     *
     * In practice this means that this will be the first area that is
     * hit tested when a pointer event arrives.
     *
     * Notice however, that this area will only keep its top priority status
     * until the next area is added to the registry, or until
     * this function is called again for some other area. I.e. there is no
     * way for giving any area a permanent status as top priority area
     * (Registry works as a stack, and new items are added on top).
     *
     * If given control pointer is NULL, this function call does nothing.
     *
     * @since S60 5.0
     * @param aControl - The control, who has registered the area.
     * @param aIndex   - The index number of the area, which
     *                   will be prioritized.
     */
    virtual void MoveFeedbackAreaToFirstPriority( const CCoeControl* aControl,
                                                  TUint32 aIndex ) = 0;

    /**
     * This function forces all registry changes made on client side to be
     * sent to server side immediately.
     *
     * This operation causes always immediate and synchronous client-server
     * transaction, and can possibly also cause flushing of window server
     * client side buffer. Hence this function should only be used in case
     * there is a synchronization problem so that feedback triggered from
     * area registry does not correspond to the areas defined by application.
     *
     * This function is only likely to help in such situation, where this
     * application's active objects are running all the time for several seconds,
     * because in that case updates won't be transferred to server immediately.
     *
     * Calling this function has no effect in case there are no pending
     * area registry updates.
     *
     * @since S60 5.0
     */
    virtual void FlushRegistryUpdates( ) = 0;

    /**
     * Gives direct feedback.
     *
     * Notice that the feedback might not be actually played, if
     * for example user has disabled the feedback from the settings.
     *
     * This function always causes a synchronous client-server transaction,
     * and potentially flushing of window server client-side buffer.
     *
     * @since S60 5.0
     * @param aType - The logical feedback type to play.
     */
    virtual void InstantFeedback( TTouchLogicalFeedback aType ) = 0;

    /**
     * Gives direct feedback if given control has not disabled it.
     *
     * This function only gives feedback, if EnableFeedbackForControl
     * function has NOT been called on given control with second
     * parameter EFalse.
     *
     * This overload is recommended when
     * feedback is triggered from UI controls, because then the owner
     * of the control can decide whether both area registry based and
     * direct feedback should be enabled or not.
     *
     * If feedback is enabled for the given control, then this function
     * causes a synchronous client-server transaction,
     * and potentially flushing of window server client-side buffer.
     *
     * @since S60 5.0
     * @param aControl - The control, who wishes to play feedback.
     * @param aType    - The logical feedback type to play.
     */
    virtual void InstantFeedback( const CCoeControl* aControl,
                                  TTouchLogicalFeedback aType ) = 0;


    /**
     * Can be used for querying, whether given control has any feedback
     * areas registered.
     *
     * It does not matter whether the areas are disabled or enabled for
     * the moment.
     *
     * Notice that one should not usually call this function, as it
     * is always allowed to call for example RemoveFeedbackArea
     * -function without checking if the area really exists (this
     * is done internally in the API implementation anyway).
     *
     * @since S60 5.0
     * @param aControl - Pointer to the control, whose feedback is
     *                   queried.
     * @return ETrue if the given control has at least one feedback
     *         area defined. EFalse otherwise.
     */
    virtual TBool ControlHasFeedback( const CCoeControl* aControl ) = 0;

    /**
     * Can be used for querying, whether given control has a feedback
     * area defined with given index number.
     *
     * It does not matter whether the areas are disabled or enabled for
     * the moment.
     *
     * Notice that one should not usually call this function, as it
     * is always allowed to call for example RemoveFeedbackArea
     * -function without checking if the area really exists (this
     * is done internally in the API implementation anyway).
     *
     * @since S60 5.0
     * @param aControl - Pointer to the control, whose feedback is
     *                   queried.
     * @param aIndex   - The index number of the area which is queried.
     * @return ETrue if the given control has a feedback area defined
     *         with given index number. EFalse otherwise.
     */
    virtual TBool ControlHasFeedback( const CCoeControl* aControl,
                                      TUint32 aIndex ) = 0;

    /**
     * This function enables or disables all feedback areas registered for
     * the given control. Disabling also affects to the overloaded version
     * of InstantFeedback -function, so that feedback is not played if
     * the control given as parameter has its feedback disabled.
     *
     * This function can be used for temporarily disabling
     * the feedback for some given control. Calling with second parameter
     * ETrue re-enables feedback areas, but it still does not
     * guarantee any feedback (control can be dimmed, invisible, of may not
     * even have any feedback areas registered).
     *
     * This function affects both vibra- and audio feedback.
     *
     * Any control that uses this function must call RemoveFeedbackForControl
     * -function in its destructor.
     *
     * @since S60 5.0
     * @param aControl - Pointer to control, whose feedback will be
     *                   enabled or disabled according to second parameter.
     * @param aEnable  - Use EFalse for temporary disabling the feedback for
     *                   this control, and ETrue for restoring the situation
     *                   to normal.
     */
    virtual void EnableFeedbackForControl( const CCoeControl* aControl,
                                           TBool aEnable ) = 0;

    /**
     * This function can be used for separately enabling or disabling audio-
     * and vibra feedback for the given control.
     *
     * Otherwise the function behaves in the same way as the overload with
     * only one TBool parameter.
     *
     * Any control that uses this function must call RemoveFeedbackForControl
     * -function in its destructor.
     *
     * @since S60 5.0
     * @param aControl - Pointer to control, whose audio- and vibra
     *                   feedback will be enabled or disabled according
     *                   to given parameters.
     * @param aEnableVibra  - Use EFalse for temporary disabling the vibra
     *                        feedback for this control, and ETrue for
     *                        restoring the situation to normal.
     * @param aEnableAudio  - Use EFalse for temporary disabling the audio
     *                        feedback for this control, and ETrue for
     *                        restoring the situation to normal.
     */
    virtual void EnableFeedbackForControl( const CCoeControl* aControl,
                                           TBool aEnableVibra,
                                           TBool aEnableAudio ) = 0;

    /**
     * Used for disabling/enabling audio/vibra feedback in
     * the application.
     *
     * This is identical with the overload which has only one parameter,
     * with the exception that one can disable audio and vibra feedback
     * separately with this version.
     *
     * @since S60 5.0
     * @param aVibraEnabled - Give ETrue as parameter for enabling vibra
     *                        feedback, and EFalse for disabling vibra
     *                        feedback for this application.
     * @param aAudioEnabled - Give ETrue as parameter for enabling audio
     *                        feedback, and EFalse for disabling audio
     *                        feedback for this application.
     */
    virtual void SetFeedbackEnabledForThisApp( TBool aVibraEnabled,
                                               TBool aAudioEnabled ) = 0;


    /**
     * Used to check whether audio or vibra feedback is enabled for this application.
     *
     * Notice that this function only returns what was given as parameter
     * to SetFeedbackEnabledForThisApp -function. I.e. this function
     * can return ETrue even if  feedback would be currently disabled
     * from settings.
     *
     * @since S60 5.2
     * @param aFeedbackType - Feedback type.
     * @return ETrue if asked feedback type is enabled for this application.
     */
    virtual TBool FeedbackEnabledForThisApp( TTouchFeedbackType aFeedbackType ) = 0;

    /**
     * Starts continuous feedback if given control has not disabled it.
     *
     * Only one continuous feedback per control can be played simultaneously.
     * Started feedback will be stopped automatically if application loses
     * key focus or application crashes or some other control starts
     * continuous feedback.
     *
     * If StartFeedback is called again for the same control, then feedback
     * type and intensity are modified accordingly. This may cause a break
     * in the feedback effect in case feedback type changes.
     *
     * EnableFeedbackForControl also affects to this function so that
     * feedback is not given if EnableFeedbackForControl
     * function has been called with second and third parameter EFalse.
     *
     * @since S60 5.2
     * @param aControl      - The control, who wishes to play feedback.
     * @param aType         - The continuous feedback type to play.
     * @param aPointerEvent - Pointer event which triggered this feedback
     *                        (give NULL as parameter if no related pointer
     *                         event available)
     * @param aIntensity    - Feedback intensity to begin with. range 0-100%.
     *                        Use 50% if no reason to use other value.
     * @param aTimeout      - Timeout value to automatically stop continuous
     *                        feedback if there's no new Start call within the
     *                        timeout. Use value 0 if timeout is not used.
     */
    virtual void StartFeedback( const CCoeControl* aControl,
                                TTouchContinuousFeedback aType,
                                const TPointerEvent* aPointerEvent,
                                TInt aIntensity,
                                TTimeIntervalMicroSeconds32 aTimeout ) = 0;


    /**
     * This function modifies intensity of continuous feedback on the fly.
     *
     * @since S60 5.2
     * @param aControl      - The control which continuous feedback is modified.
     * @param aIntensity    - New intensity value. range 0-100%
     */
    virtual void ModifyFeedback( const CCoeControl* aControl,
                                 TInt aIntensity ) = 0;


    /**
     * This function stops continuous feedback.
     *
     * @since S60 5.2
     * @param aControl - The control which continuous feedback is stopped.
     */
    virtual void StopFeedback( const CCoeControl* aControl ) = 0;


    /**
     * This function enables or disables audio or/and vibra feedback in
     * whole device.
     *
     * Tactile feedback is enabled by default, and thus standard
     * S60 components (such as CBA, lists and options menu) automatically
     * give feedback even if the application itself would make no effort
     * for producing feedback.
     *
     * Requires WriteDeviceData capability
     *
     * @since S60 5.2
     * @param aFeedbackType - Feedback types to be enabled/disabled defined as
     *                        a bitmask combination of enumeration items from
     *                        TTouchFeedbackType
     * @return KErrNone, or one of standard Symbian OS error codes
     *         if user has not WriteDeviceData capability.
     */
    virtual TInt SetFeedbackEnabledForDevice( TTouchFeedbackType aFeedbackType ) = 0;


    /**
     * Used to check enabled feedback types for the device.
     *
     * Notice that this function only returns what was given as parameter
     * to SetFeedbackEnabledForDevice -function. I.e. this function
     * can return ETrue even if  feedback would be currently disabled
     * from settings.
     *
     * @since S60 5.2
     * @return Enabled/disabled feedback types as bitmask combination.
     */
    virtual TTouchFeedbackType FeedbackEnabledForDevice() = 0;


    /**
     * Gives direct feedback if given control has not disabled it.
     *
     * This overload is recommended when triggered feedback is related
     * to pointer event. System uses aPointerEvent to avoiding
     * two (or more) direct feedbacks for the same pointer event.
     *
     * @since S60 5.2
     * @param aControl      - The control, who wishes to play feedback.
     * @param aType         - The logical feedback type to play.
     * @param aPointerEvent - pointer event which triggered this feedback.
     */
    virtual void InstantFeedback( const CCoeControl* aControl,
                                  TTouchLogicalFeedback aType,
                                  const TPointerEvent& aPointerEvent ) = 0;


    /**
     * Sets or updates rectangular feedback area to registry.
     *
     * This overload allows user to set more than one touch event types to
     * one feedback area. Otherwise the function behaves in the same way
     * as the overload with separated feedback type and event type parameters.
     *
     * @since S60 5.2
     * @param aControl        - The control handling pointer events on this
     *                          feedback area.
     * @param aIndex          - The index number of the area to be added.
     * @param aRect           - The feedback area rectangle.
     * @param aFeedbackSpec   - The specification how this feedback area
     *                          triggers feedback for different touch events.
     * @return KErrNone, or one of standard Symbian OS error codes
     *         if setting of area to registry failed.
     *         Some specific error codes:
     *         KErrArgument - A NULL pointer was given as first parameter, or
     *                        the given control does not have any window
     *                        associated to it.
     *         KErrNotSupported - Unsupported predefined feedback profile
     *                            was given as parameter.
     */
    virtual TInt SetFeedbackArea( const CCoeControl* aControl,
                                  TUint32 aIndex,
                                  TRect aRect,
                                  CFeedbackSpec* aFeedbackSpec ) = 0;

    /**
     * Gives direct feedback if given control has not disabled it.
     *
     * This overload is recommended when triggered feedback is related
     * to pointer event. System uses aPointerEvent to avoiding
     * two (or more) direct feedbacks for the same pointer event. Using this
     * overload it is also possible to disable unwanted feedback (vibra/audio)
     * by giving only wanted feedback type as parameter.
     *
     * @since S60 5.2
     * @param aControl      - The control, who wishes to play feedback.
     * @param aType         - The logical feedback type to play.
     * @param aFeedbackType - Feedback types to be played as a bitmask
     *                        combination of enumeration items from
     *                        TTouchFeedbackType
     * @param aPointerEvent - Pointer event, which triggered this feedback.
     */
    virtual void InstantFeedback( const CCoeControl* aControl,
                                  TTouchLogicalFeedback aType,
                                  TTouchFeedbackType aFeedbackType,
                                  const TPointerEvent& aPointerEvent ) = 0;

    /**
     * Gives direct feedback.
     *
     * Notice that the feedback might not be actually played, if
     * for example user has disabled the feedback from the settings.
     *
     * This function always causes a synchronous client-server transaction,
     * and potentially flushing of window server client-side buffer. Using this
     * overload it is also possible to disable unwanted feedback (vibra/audio)
     * by giving only wanted feedback type as parameter.
     *
     * @since S60 5.2
     * @param aType         - The logical feedback type to play.
     * @param aFeedbackType - Feedback types to be played as a bitmask
     *                        combination of enumeration items from
     *                        TTouchFeedbackType
     */
    virtual void InstantFeedback( TTouchLogicalFeedback aType,
                                  TTouchFeedbackType aFeedbackType ) = 0;


    };



#endif //  M_TOUCHFEEDBACK_H
