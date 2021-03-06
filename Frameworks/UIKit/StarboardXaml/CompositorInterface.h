//******************************************************************************
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************
#pragma once
#include <deque>
#include <map>

#include "CACompositor.h"
#include "winobjc\winobjc.h"
#include <ppltasks.h>

#ifdef __clang__
#include <COMIncludes.h>
#endif

#include <DWrite_3.h>

#ifdef __clang__
#include <COMIncludes_End.h>
#endif

class DisplayNode;
class DisplayTexture;
class DisplayAnimation;

typedef enum {
    CompositionModeDefault = 0,
    CompositionModeLibrary = 1,
} CompositionMode;

void CreateXamlCompositor(winobjc::Id& root, CompositionMode compositionMode);

template <class T>
class RefCounted;

class RefCountedType {
    template <class T>
    friend class RefCounted;
    int refCount;

protected:
    void AddRef();
    void Release();

    RefCountedType();
    virtual ~RefCountedType();
};

template <class T>
class RefCounted {
private:
    T* _ptr;

public:
    RefCounted() {
        _ptr = NULL;
    }
    RefCounted(T* ptr) {
        _ptr = ptr;
        if (_ptr)
            _ptr->AddRef();
    }

    RefCounted(const RefCounted& copy) {
        _ptr = copy._ptr;
        if (_ptr)
            _ptr->AddRef();
    }

    ~RefCounted() {
        if (_ptr)
            _ptr->Release();
        _ptr = NULL;
    }
    RefCounted& operator=(const RefCounted& val) {
        if (_ptr == val._ptr)
            return *this;
        T* oldPtr = _ptr;
        _ptr = val._ptr;
        if (_ptr)
            _ptr->AddRef();
        if (oldPtr)
            oldPtr->Release();

        return *this;
    }

    T* operator->() {
        return _ptr;
    }

    T* Get() {
        return _ptr;
    }

    operator bool() {
        return _ptr != NULL;
    }

    bool operator<(const RefCounted& other) const {
        return _ptr < other._ptr;
    }
};

typedef RefCounted<DisplayNode> DisplayNodeRef;
typedef RefCounted<DisplayTexture> DisplayTextureRef;
typedef RefCounted<DisplayAnimation> DisplayAnimationRef;

class DisplayAnimation : public RefCountedType {
    friend class CAXamlCompositor;

public:
    winobjc::Id _xamlAnimation;
    enum Easing { EaseInEaseOut, EaseIn, EaseOut, Linear, Default };

    double beginTime;
    double duration;
    bool autoReverse;
    float repeatCount;
    float repeatDuration;
    float speed;
    double timeOffset;
    Easing easingFunction;

    DisplayAnimation();
    virtual ~DisplayAnimation();

    virtual void Completed() = 0;
    virtual concurrency::task<void> AddToNode(DisplayNode* node) = 0;

    void CreateXamlAnimation();
    void Start();
    void Stop();

    concurrency::task<void> AddAnimation(
        DisplayNode* node, const wchar_t* propertyName, bool fromValid, float from, bool toValid, float to);
    concurrency::task<void> AddTransitionAnimation(DisplayNode* node, const char* type, const char* subtype);
};

class DisplayTexture : public RefCountedType {
    friend class CAXamlCompositor;

public:
    virtual ~DisplayTexture(){};
    virtual void SetNodeContent(DisplayNode* node, float contentWidth, float contentHeight, float contentScale) = 0;
};

class DisplayTextureXamlGlyphs : public DisplayTexture {
public:
    winobjc::Id _xamlTextbox;

    enum DisplayTextureTextHAlignment { alignLeft, alignCenter, alignRight };

    DisplayTextureTextHAlignment _horzAlignment;
    float _insets[4];
    float _color[4];
    float _fontSize;
    float _lineHeight;
    bool _centerVertically;
    DWRITE_FONT_WEIGHT _fontWeight;
    DWRITE_FONT_STRETCH _fontStretch;
    DWRITE_FONT_STYLE _fontStyle;

    DisplayTextureXamlGlyphs();
    ~DisplayTextureXamlGlyphs();

    float _desiredWidth, _desiredHeight;
    void Measure(float width, float height);
    void ConstructGlyphs(const Microsoft::WRL::Wrappers::HString& fontFamilyName, const wchar_t* str, int len);
    void SetNodeContent(DisplayNode* node, float width, float height, float scale);
};

#include <set>

class CAXamlCompositor;

class DisplayNode : public RefCountedType {
    friend class CAXamlCompositor;

public:
    bool isRoot;
    DisplayNode* parent;
    std::set<DisplayNodeRef> _subnodes;
    DisplayTextureRef currentTexture;
    bool topMost;

    // A DisplayNode map to a visual control which participates in layout. During layout, a display node will be positioned
    // within its parent display node. Also, The visual control in a display node must maintain and arrange visual elements
    // represented by child display nodes
    //
    // The visual control can be simple control control, etc Canvas, Grid.  In this case, the control itself is naturally
    // used for participating layout within its parent visual control as well as laying out its child visual elements purpose.
    //
    // For complex (or composite) control, e.g., UIScrollView - which is a control with several layer, e.g.,
    // an backing panel behind, the scrollViewer in the middle and the cavas on the top layer. The backing panel should particiate
    // the layout with its parent. But it is the canvas is responsbile for laying out child visual elements represented
    // by the child display node
    //
    // For this reason, we differentiate two types of visual elements for a DisplayNode - the layoutElement and the
    // contentElement. For simple case, both refer to the same visual element. For compoiste control case, they refer to
    // different visual elements.
    //
    // Conceptually, you can consider layoutElement as the root visual layer in a display node, and contentElement as
    // the top most visual layer in the display node. Both has implicit requirements - they must be sublcass of Panel

    winobjc::Id _layoutElement; // used for layout with parent of the display node
    winobjc::Id _contentElement; // used for layout with the children of the display node

public:
    DisplayNode();
    virtual ~DisplayNode();

    void SetTopMost();
    concurrency::task<void> AddAnimation(DisplayAnimation* animation);
    void SetProperty(const wchar_t* name, float value);
    void SetPropertyInt(const wchar_t* name, int value);
    void SetHidden(bool hidden);
    void SetMasksToBounds(bool masksToBounds);
    void SetBackgroundColor(float r, float g, float b, float a);
    void SetContentsCenter(float x, float y, float width, float height);
    void SetContents(winobjc::Id& bitmap, float width, float height, float scale);
    void SetContents(DisplayTexture* tex, float width, float height, float scale);
    void SetContentsElement(winobjc::Id& elem, float width, float height, float scale);
    void SetContentsElement(winobjc::Id& elem);

    void SetShouldRasterize(bool shouldRasterize);

    // Setting the root control and the content control for scrollviewer
    // this is needed because root control will be the first child of CALayerXaml
    // and the content control will be the one to layout children
    void SetScrollviewerControls(IInspectable* rootControl, IInspectable* contentControl);

    float GetPresentationPropertyValue(const char* name);
    void AddToRoot();
    void SetNodeZIndex(int zIndex);

    virtual void* GetProperty(const char* name) = 0;
    virtual void UpdateProperty(const char* name, void* value) = 0;
    void AddSubnode(DisplayNode* node, DisplayNode* before, DisplayNode* after);
    void MoveNode(DisplayNode* before, DisplayNode* after);
    void RemoveFromSupernode();
};

struct ICompositorTransaction {
public:
    virtual ~ICompositorTransaction() {
    }
    virtual void Process() = 0;
};

struct ICompositorAnimationTransaction {
public:
    virtual ~ICompositorAnimationTransaction() {
    }
    virtual concurrency::task<void> Process() = 0;
};

// Dispatches the compositor transactions that have been queued up
void DispatchCompositorTransactions(
    std::deque<std::shared_ptr<ICompositorTransaction>>&& subTransactions,
    std::deque<std::shared_ptr<ICompositorAnimationTransaction>>&& animationTransactions,
    std::map<DisplayNode*, std::map<std::string, std::shared_ptr<ICompositorTransaction>>>&& propertyTransactions,
    std::deque<std::shared_ptr<ICompositorTransaction>>&& movementTransactions);
