Imports System.Runtime.InteropServices
Imports System.Drawing
Imports System.Runtime.CompilerServices
Imports System.IO
Imports Microsoft.Win32

Module modWinAPI
    Declare Function SetWindowText Lib "user32" Alias "SetWindowTextA" (ByVal hWnd As IntPtr, ByVal lpString As String) As Boolean
    Declare Function ShowWindow Lib "user32" Alias "ShowWindow" (ByVal hwnd As IntPtr, ByVal nCmdShow As Integer) As Boolean
    Public mbDebugging As Boolean

    Public Const PROCESSBASICINFORMATION As UInteger = 0
    Declare Function SetWindowPos5555 Lib "user32" ( _
    ByVal hwnd As Long, _
    ByVal hWndInsertAfter As Long, _
    ByVal x As Long, _
    ByVal y As Long, _
    ByVal cx As Long, _
    ByVal cy As Long, _
    ByVal wFlags As Long _
    ) As Long
    Public Const HWND_TOPMOST = -1
    Public Const SWP_NOACTIVATE = &H10
    Public Const GWL_STYLE = -16
    Public Const WS_MINIMIZEBOX As Long = &H20000
    Public Const WS_MAXIMIZEBOX As Long = &H10000

    Const HWND_NOTOPMOST = -2


    <System.Runtime.InteropServices.DllImport("ntdll.dll", EntryPoint:="NtQueryInformationProcess")> _
    Public Function NtQueryInformationProcess(ByVal handle As IntPtr, ByVal processinformationclass As UInteger, ByRef ProcessInformation As Process_Basic_Information, ByVal ProcessInformationLength As Integer, ByRef ReturnLength As UInteger) As Integer
    End Function

    <System.Runtime.InteropServices.StructLayoutAttribute(System.Runtime.InteropServices.LayoutKind.Sequential, Pack:=1)> _
    Public Structure Process_Basic_Information
        Public ExitStatus As IntPtr
        Public PepBaseAddress As IntPtr
        Public AffinityMask As IntPtr
        Public BasePriority As IntPtr
        Public UniqueProcessID As IntPtr
        Public InheritedFromUniqueProcessId As IntPtr
    End Structure

    Public Function AllowWindowsAppsToCrashWithoutErrorReportingDialog() As String

        Try

            '[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting]
            '"ForceQueue"=dword:00000001
            Dim RegKey As RegistryKey
            RegKey = Registry.LocalMachine.OpenSubKey("Software\Microsoft\Windows\Windows Error Reporting", True)
            RegKey.SetValue("ForceQueue", 1)
            Dim dFQ As Double = 0

            dFQ = RegKey.GetValue("ForceQueue", -1)
            If dFQ <> 1 Then MsgBox("Unable to set Key", MsgBoxStyle.Critical, "Registry Editor")
            RegKey.Close()

            '[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\Consent]
            '"DefaultConsent"=dword:00000001
            RegKey = Registry.LocalMachine.OpenSubKey("Software\Microsoft\Windows\Windows Error Reporting\Consent", True)
            RegKey.SetValue("DefaultConsent", 1)
            dFQ = RegKey.GetValue("DefaultConsent", -1)
            If dFQ <> 1 Then MsgBox("Unable to Set Key", MsgBoxStyle.Critical, "Registry Editor")
            RegKey.Close()

            MsgBox("Successfully set Windows Error Reporting behavior keys.", MsgBoxStyle.Exclamation, "Registry Editor")
            Return "SUCCESS"
        Catch ex As Exception
            MsgBox(ex.Message, MsgBoxStyle.Critical, "Unable to Set Registry Key")
            Exit Function
        End Try

    End Function
    Public Function DefaultHostName(sDefault As String, bOverride As Boolean) As String
        Dim sPersisted As String = KeyValue(sDefault)
        If Len(sPersisted) = 0 Then sDefault = Replace(sDefault, "p2psql", "pool")
        If mbDebugging Then Return "localhost"
        Return sDefault
    End Function
    Public Function DefaultPort(lPort As Long, bOverride As Boolean) As Long
        Dim lPersisted As Long = Val("0" & KeyValue("p2pport"))
        If lPersisted = 0 Then lPort = 80
        If mbDebugging Then Return 7777
        Return lPort
    End Function

    Public Enum WindowStylesEx As UInteger
        ''' <summary>
        ''' Specifies that a window created with this style accepts drag-drop files.
        ''' </summary>
        WS_EX_ACCEPTFILES = &H10
        ''' <summary>
        ''' Forces a top-level window onto the taskbar when the window is visible.
        ''' </summary>
        WS_EX_APPWINDOW = &H40000
        ''' <summary>
        ''' Specifies that a window has a border with a sunken edge.
        ''' </summary>
        WS_EX_CLIENTEDGE = &H200
        ''' <summary>
        ''' Windows XP: Paints all descendants of a window in bottom-to-top painting order using double-buffering. For more information, see Remarks. This cannot be used if the window has a class style of either CS_OWNDC or CS_CLASSDC. 
        ''' </summary>
        WS_EX_COMPOSITED = &H2000000
        ''' <summary>
        ''' Includes a question mark in the title bar of the window. When the user clicks the question mark, the cursor changes to a question mark with a pointer. If the user then clicks a child window, the child receives a WM_HELP message. The child window should pass the message to the parent window procedure, which should call the WinHelp function using the HELP_WM_HELP command. The Help application displays a pop-up window that typically contains help for the child window.
        ''' WS_EX_CONTEXTHELP cannot be used with the WS_MAXIMIZEBOX or WS_MINIMIZEBOX styles.
        ''' </summary>
        WS_EX_CONTEXTHELP = &H400
        ''' <summary>
        ''' The window itself contains child windows that should take part in dialog box navigation. If this style is specified, the dialog manager recurses into children of this window when performing navigation operations such as handling the TAB key, an arrow key, or a keyboard mnemonic.
        ''' </summary>
        WS_EX_CONTROLPARENT = &H10000
        ''' <summary>
        ''' Creates a window that has a double border; the window can, optionally, be created with a title bar by specifying the WS_CAPTION style in the dwStyle parameter.
        ''' </summary>
        WS_EX_DLGMODALFRAME = &H1
        ''' <summary>
        ''' Windows 2000/XP: Creates a layered window. Note that this cannot be used for child windows. Also, this cannot be used if the window has a class style of either CS_OWNDC or CS_CLASSDC. 
        ''' </summary>
        WS_EX_LAYERED = &H80000
        ''' <summary>
        ''' Arabic and Hebrew versions of Windows 98/Me, Windows 2000/XP: Creates a window whose horizontal origin is on the right edge. Increasing horizontal values advance to the left. 
        ''' </summary>
        WS_EX_LAYOUTRTL = &H400000
        ''' <summary>
        ''' Creates a window that has generic left-aligned properties. This is the default.
        ''' </summary>
        WS_EX_LEFT = &H0
        ''' <summary>
        ''' If the shell language is Hebrew, Arabic, or another language that supports reading order alignment, the vertical scroll bar (if present) is to the left of the client area. For other languages, the style is ignored.
        ''' </summary>
        WS_EX_LEFTSCROLLBAR = &H4000
        ''' <summary>
        ''' The window text is displayed using left-to-right reading-order properties. This is the default.
        ''' </summary>
        WS_EX_LTRREADING = &H0
        ''' <summary>
        ''' Creates a multiple-document interface (MDI) child window.
        ''' </summary>
        WS_EX_MDICHILD = &H40
        ''' <summary>
        ''' Windows 2000/XP: A top-level window created with this style does not become the foreground window when the user clicks it. The system does not bring this window to the foreground when the user minimizes or closes the foreground window. 
        ''' To activate the window, use the SetActiveWindow or SetForegroundWindow function.
        ''' The window does not appear on the taskbar by default. To force the window to appear on the taskbar, use the WS_EX_APPWINDOW style.
        ''' </summary>
        WS_EX_NOACTIVATE = &H8000000
        ''' <summary>
        ''' Windows 2000/XP: A window created with this style does not pass its window layout to its child windows.
        ''' </summary>
        WS_EX_NOINHERITLAYOUT = &H100000
        ''' <summary>
        ''' Specifies that a child window created with this style does not send the WM_PARENTNOTIFY message to its parent window when it is created or destroyed.
        ''' </summary>
        WS_EX_NOPARENTNOTIFY = &H4
        ''' <summary>
        ''' Combines the WS_EX_CLIENTEDGE and WS_EX_WINDOWEDGE styles.
        ''' </summary>
        WS_EX_OVERLAPPEDWINDOW = WS_EX_WINDOWEDGE Or WS_EX_CLIENTEDGE
        ''' <summary>
        ''' Combines the WS_EX_WINDOWEDGE, WS_EX_TOOLWINDOW, and WS_EX_TOPMOST styles.
        ''' </summary>
        WS_EX_PALETTEWINDOW = WS_EX_WINDOWEDGE Or WS_EX_TOOLWINDOW Or WS_EX_TOPMOST
        ''' <summary>
        ''' The window has generic "right-aligned" properties. This depends on the window class. This style has an effect only if the shell language is Hebrew, Arabic, or another language that supports reading-order alignment; otherwise, the style is ignored.
        ''' Using the WS_EX_RIGHT style for static or edit controls has the same effect as using the SS_RIGHT or ES_RIGHT style, respectively. Using this style with button controls has the same effect as using BS_RIGHT and BS_RIGHTBUTTON styles.
        ''' </summary>
        WS_EX_RIGHT = &H1000
        ''' <summary>
        ''' Vertical scroll bar (if present) is to the right of the client area. This is the default.
        ''' </summary>
        WS_EX_RIGHTSCROLLBAR = &H0
        ''' <summary>
        ''' If the shell language is Hebrew, Arabic, or another language that supports reading-order alignment, the window text is displayed using right-to-left reading-order properties. For other languages, the style is ignored.
        ''' </summary>
        WS_EX_RTLREADING = &H2000
        ''' <summary>
        ''' Creates a window with a three-dimensional border style intended to be used for items that do not accept user input.
        ''' </summary>
        WS_EX_STATICEDGE = &H20000
        ''' <summary>
        ''' Creates a tool window; that is, a window intended to be used as a floating toolbar. A tool window has a title bar that is shorter than a normal title bar, and the window title is drawn using a smaller font. A tool window does not appear in the taskbar or in the dialog that appears when the user presses ALT+TAB. If a tool window has a system menu, its icon is not displayed on the title bar. However, you can display the system menu by right-clicking or by typing ALT+SPACE. 
        ''' </summary>
        WS_EX_TOOLWINDOW = &H80
        ''' <summary>
        ''' Specifies that a window created with this style should be placed above all non-topmost windows and should stay above them, even when the window is deactivated. To add or remove this style, use the etWindowPos function.
        ''' </summary>
        WS_EX_TOPMOST = &H8
        ''' <summary>
        ''' Specifies that a window created with this style should not be painted until siblings beneath the window (that were created by the same thread) have been painted. The window appears transparent because the bits of underlying sibling windows have already been painted.
        ''' To achieve transparency without these restrictions, use the SetWindowRgn function.
        ''' </summary>
        WS_EX_TRANSPARENT = &H20
        ''' <summary>
        ''' Specifies that a window has a border with a raised edge.
        ''' </summary>
        WS_EX_WINDOWEDGE = &H100
    End Enum

    <DllImport("User32.dll")> _
    Public Function SendMessage(ByVal hWnd As IntPtr, ByVal msg As Integer, ByVal wParam As IntPtr, ByVal lParam As IntPtr) As Integer

    End Function

    <DllImport("user32.dll")> _
    Public Function AdjustWindowRectEx(<MarshalAs(UnmanagedType.Struct)> ByRef lpRect As RECT, _
                     <MarshalAs(UnmanagedType.U4)> ByVal dwStyle As WindowStyles, _
                     <MarshalAs(UnmanagedType.Bool)> ByVal bMenu As Boolean, _
                     <MarshalAs(UnmanagedType.U4)> ByVal dwExStyle As WindowStylesEx) As <MarshalAs(UnmanagedType.Bool)> Boolean
    End Function

    <DllImport("user32.dll")> _
    Public Function MoveWindow(ByVal hWnd As IntPtr, ByVal x As Integer, ByVal y As Integer, ByVal nWidth As Integer, ByVal nHeight As Integer, ByVal bRepaint As Boolean) As Boolean
    End Function

    <DllImport("user32.dll", SetLastError:=True, CharSet:=CharSet.Auto)> _
    Public Function SetParent(ByVal hWndChild As IntPtr, ByVal hWndNewParent As IntPtr) As IntPtr
    End Function

    <DllImport("user32.dll", SetLastError:=True, CharSet:=CharSet.Auto)> _
    Private Function FindWindow( _
     ByVal lpClassName As String, _
     ByVal lpWindowName As String) As IntPtr
    End Function

    <DllImport("user32.dll", EntryPoint:="FindWindow", SetLastError:=True)> _
    Public Function FindWindowByCaption(ByVal zeroOnly As IntPtr, ByVal lpWindowName As String) As IntPtr
    End Function

    <System.Runtime.InteropServices.DllImport("user32.dll")> _
    Private Function GetSystemMenu(ByVal hWnd As IntPtr, ByVal bRevert As Boolean) As IntPtr
    End Function
    <System.Runtime.InteropServices.DllImport("user32.dll")> _
    Private Function GetMenuItemCount(ByVal hMenu As IntPtr) As Integer
    End Function
    <System.Runtime.InteropServices.DllImport("user32.dll")> _
    Private Function DrawMenuBar(ByVal hWnd As IntPtr) As Boolean
    End Function
    <System.Runtime.InteropServices.DllImport("user32.dll")> _
    Private Function RemoveMenu(ByVal hMenu As IntPtr, ByVal uPosition As UInteger, ByVal uFlags As UInteger) As Boolean
    End Function
    Private Const MF_BYPOSITION As Int32 = &H400
    Private Const MF_REMOVE As Int32 = &H1000


    <StructLayout(LayoutKind.Sequential)> _
    Public Structure RECT
        Private _Left As Integer, _Top As Integer, _Right As Integer, _Bottom As Integer

        Public Sub New(ByVal Rectangle As Rectangle)
            Me.New(Rectangle.Left, Rectangle.Top, Rectangle.Right, Rectangle.Bottom)
        End Sub
        Public Sub New(ByVal Left As Integer, ByVal Top As Integer, ByVal Right As Integer, ByVal Bottom As Integer)
            _Left = Left
            _Top = Top
            _Right = Right
            _Bottom = Bottom
        End Sub

        Public Property X As Integer
            Get
                Return _Left
            End Get
            Set(ByVal value As Integer)
                _Right = _Right - _Left + value
                _Left = value
            End Set
        End Property
        Public Property Y As Integer
            Get
                Return _Top
            End Get
            Set(ByVal value As Integer)
                _Bottom = _Bottom - _Top + value
                _Top = value
            End Set
        End Property
        Public Property Left As Integer
            Get
                Return _Left
            End Get
            Set(ByVal value As Integer)
                _Left = value
            End Set
        End Property
        Public Property Top As Integer
            Get
                Return _Top
            End Get
            Set(ByVal value As Integer)
                _Top = value
            End Set
        End Property
        Public Property Right As Integer
            Get
                Return _Right
            End Get
            Set(ByVal value As Integer)
                _Right = value
            End Set
        End Property
        Public Property Bottom As Integer
            Get
                Return _Bottom
            End Get
            Set(ByVal value As Integer)
                _Bottom = value
            End Set
        End Property
        Public Property Height() As Integer
            Get
                Return _Bottom - _Top
            End Get
            Set(ByVal value As Integer)
                _Bottom = value + _Top
            End Set
        End Property
        Public Property Width() As Integer
            Get
                Return _Right - _Left
            End Get
            Set(ByVal value As Integer)
                _Right = value + _Left
            End Set
        End Property
        Public Property Location() As POINT
            Get
                Return New POINT(Left, Top)
            End Get
            Set(ByVal value As POINT)
                _Right = _Right - _Left + value.X
                _Bottom = _Bottom - _Top + value.Y
                _Left = value.X
                _Top = value.Y
            End Set
        End Property
        Public Property Size() As Size
            Get
                Return New Size(Width, Height)
            End Get
            Set(ByVal value As Size)
                _Right = value.Width + _Left
                _Bottom = value.Height + _Top
            End Set
        End Property


        <System.Runtime.InteropServices.StructLayout(Runtime.InteropServices.LayoutKind.Sequential)> _
        Public Structure POINT
            Public X As Integer
            Public Y As Integer

            Public Sub New(ByVal X As Integer, ByVal Y As Integer)
                Me.X = X
                Me.Y = Y
            End Sub
        End Structure
        Public Shared Widening Operator CType(ByVal Rectangle As RECT) As Rectangle
            Return New Rectangle(Rectangle.Left, Rectangle.Top, Rectangle.Width, Rectangle.Height)
        End Operator
        Public Shared Widening Operator CType(ByVal Rectangle As Rectangle) As RECT
            Return New RECT(Rectangle.Left, Rectangle.Top, Rectangle.Right, Rectangle.Bottom)
        End Operator
        Public Shared Operator =(ByVal Rectangle1 As RECT, ByVal Rectangle2 As RECT) As Boolean
            Return Rectangle1.Equals(Rectangle2)
        End Operator
        Public Shared Operator <>(ByVal Rectangle1 As RECT, ByVal Rectangle2 As RECT) As Boolean
            Return Not Rectangle1.Equals(Rectangle2)
        End Operator

        Public Overrides Function ToString() As String
            Return "{Left: " & _Left & "; " & "Top: " & _Top & "; Right: " & _Right & "; Bottom: " & _Bottom & "}"
        End Function

        Public Overloads Function Equals(ByVal Rectangle As RECT) As Boolean
            Return Rectangle.Left = _Left AndAlso Rectangle.Top = _Top AndAlso Rectangle.Right = _Right AndAlso Rectangle.Bottom = _Bottom
        End Function
        Public Overloads Overrides Function Equals(ByVal [Object] As Object) As Boolean
            If TypeOf [Object] Is RECT Then
                Return Equals(DirectCast([Object], RECT))
            ElseIf TypeOf [Object] Is Rectangle Then
                Return Equals(New RECT(DirectCast([Object], Rectangle)))
            End If

            Return False
        End Function
    End Structure

    <Flags()> Public Enum WindowStyles As UInteger
        ''' <summary>The window has a thin-line border.</summary>
        WS_BORDER = &H800000

        ''' <summary>The window has a title bar (includes the WS_BORDER style).</summary>
        WS_CAPTION = &HC00000

        ''' <summary>The window is a child window. A window with this style cannot have a menu bar. This style cannot be used with the WS_POPUP style.</summary>
        WS_CHILD = &H40000000

        ''' <summary>Excludes the area occupied by child windows when drawing occurs within the parent window. This style is used when creating the parent window.</summary>
        WS_CLIPCHILDREN = &H2000000

        ''' <summary>
        ''' Clips child windows relative to each other; that is, when a particular child window receives a WM_PAINT message, the WS_CLIPSIBLINGS style clips all other overlapping child windows out of the region of the child window to be updated.
        ''' If WS_CLIPSIBLINGS is not specified and child windows overlap, it is possible, when drawing within the client area of a child window, to draw within the client area of a neighboring child window.
        ''' </summary>
        WS_CLIPSIBLINGS = &H4000000

        ''' <summary>The window is initially disabled. A disabled window cannot receive input from the user. To change this after a window has been created, use the EnableWindow function.</summary>
        WS_DISABLED = &H8000000

        ''' <summary>The window has a border of a style typically used with dialog boxes. A window with this style cannot have a title bar.</summary>
        WS_DLGFRAME = &H400000

        ''' <summary>
        ''' The window is the first control of a group of controls. The group consists of this first control and all controls defined after it, up to the next control with the WS_GROUP style.
        ''' The first control in each group usually has the WS_TABSTOP style so that the user can move from group to group. The user can subsequently change the keyboard focus from one control in the group to the next control in the group by using the direction keys.
        ''' You can turn this style on and off to change dialog box navigation. To change this style after a window has been created, use the SetWindowLong
        '''  function.
        ''' </summary>
        WS_GROUP = &H20000

        ''' <summary>The window has a horizontal scroll bar.</summary>
        WS_HSCROLL = &H100000

        ''' <summary>The window is initially maximized.</summary> 
        WS_MAXIMIZE = &H1000000

        ''' <summary>The window has a maximize button. Cannot be combined with the WS_EX_CONTEXTHELP style. The WS_SYSMENU style must also be specified.</summary> 
        WS_MAXIMIZEBOX = &H10000

        ''' <summary>The window is initially minimized.</summary>
        WS_MINIMIZE = &H20000000

        ''' <summary>The window has a minimize button. Cannot be combined with the WS_EX_CONTEXTHELP style. The WS_SYSMENU style must also be specified.</summary>
        WS_MINIMIZEBOX = &H20000

        ''' <summary>The window is an overlapped window. An overlapped window has a title bar and a border.</summary>
        WS_OVERLAPPED = &H0

        ''' <summary>The window is an overlapped window.</summary>
        WS_OVERLAPPEDWINDOW = WS_OVERLAPPED Or WS_CAPTION Or WS_SYSMENU Or WS_SIZEFRAME Or WS_MINIMIZEBOX Or WS_MAXIMIZEBOX

        ''' <summary>The window is a pop-up window. This style cannot be used with the WS_CHILD style.</summary>
        WS_POPUP = &H80000000UI

        ''' <summary>The window is a pop-up window. The WS_CAPTION and WS_POPUPWINDOW styles must be combined to make the window menu visible.</summary>
        WS_POPUPWINDOW = WS_POPUP Or WS_BORDER Or WS_SYSMENU

        ''' <summary>The window has a sizing border.</summary>
        WS_SIZEFRAME = &H40000

        ''' <summary>The window has a window menu on its title bar. The WS_CAPTION style must also be specified.</summary>
        WS_SYSMENU = &H80000

        ''' <summary>
        ''' The window is a control that can receive the keyboard focus when the user presses the TAB key.
        ''' Pressing the TAB key changes the keyboard focus to the next control with the WS_TABSTOP style.  
        ''' You can turn this style on and off to change dialog box navigation. To change this style after a window has been created, use the etWindowLong function.
        ''' For user-created windows and modeless dialogs to work with tab stops, alter the message loop to call the IsDialogMessage function.
        ''' </summary>
        WS_TABSTOP = &H10000

        ''' <summary>The window is initially visible. This style can be turned on and off by using the ShowWindow or etWindowPos function.</summary>
        WS_VISIBLE = &H10000000

        ''' <summary>The window has a vertical scroll bar.</summary>
        WS_VSCROLL = &H200000
    End Enum

    'See: System.Windows.Forms.SafeNativeMethods.etWindowPos
    <DllImport("user32.dll", CharSet:=CharSet.Auto, ExactSpelling:=True)> _
    Private Function SetWindowPos33(ByVal hWnd As HandleRef, ByVal hWndInsertAfter As HandleRef, ByVal x As Integer, ByVal y As Integer, ByVal cx As Integer, ByVal cy As Integer, ByVal flags As Integer) As Boolean
    End Function

    'See: System.Windows.Forms.UnSafeNativeMethods.GetWindowLong*
    <DllImport("user32.dll")> _
    Private Function GetWindowLong(ByVal hWnd As IntPtr, ByVal nIndex As Integer) As IntPtr
    End Function

    <DllImport("user32.dll", EntryPoint:="GetWindowLongPtr", CharSet:=CharSet.Auto)> _
    Private Function GetWindowLongPtr64(ByVal hWnd As HandleRef, ByVal nIndex As Integer) As IntPtr
    End Function


    Public Const SWP_NOSIZE As Int32 = &H1
    Public Const SWP_NOMOVE As Int32 = &H2

    <DllImport( _
    "user32.dll", _
    CharSet:=CharSet.Auto, _
    CallingConvention:=CallingConvention.StdCall _
    )> _
    Public Function SetWindowPos( _
 ByVal hWnd As IntPtr, _
 ByVal hWndInsertAfter As IntPtr, _
 ByVal X As Int32, _
 ByVal Y As Int32, _
 ByVal cx As Int32, _
 ByVal cy As Int32, _
 ByVal uFlags As Int32) _
 As Boolean
    End Function


    <DllImport("user32.dll")> _
    Private Function SetWindowLong(ByVal hWnd As IntPtr, ByVal nIndex As Integer, ByVal value As Integer) As IntPtr
    End Function

    Public Sub RemoveTitleBar(ByVal handle As IntPtr)
        Dim hMenu As IntPtr
        Dim n As Integer
        hMenu = GetSystemMenu(handle, False)
        If hMenu <> IntPtr.Zero Then
            n = GetMenuItemCount(hMenu)
            If n > 0 Then
                RemoveMenu(hMenu, CUInt(n - 1), MF_BYPOSITION Or MF_REMOVE)
                RemoveMenu(hMenu, CUInt(n - 2), MF_BYPOSITION Or MF_REMOVE)
                RemoveMenu(hMenu, CUInt(n - 3), MF_BYPOSITION Or MF_REMOVE)
                DrawMenuBar(handle)
                Dim hLong As Long
                hLong = GetWindowLong(handle, GWL_STYLE)
                Dim flags As UInt32 = SWP_NOMOVE Or SWP_NOSIZE
                SetWindowLong(hLong, GWL_STYLE, WS_MINIMIZEBOX Or WS_MAXIMIZEBOX)
                SetWindowPos(handle, HWND_TOPMOST, 5, 5, 0, 0, flags)
                Dim style As Long = GetWindowLong(handle, -16L)
                style = style And -12582913L
                SetWindowLong(handle, -16L, style)
                SetWindowPos(handle, 7L, 0L, 0L, 0L, 0L, &H27L)
            End If
        End If
    End Sub


    Public Function KillProcess(ByVal sWildcard As String)
        Dim sCheck As String = Replace(LCase(sWildcard), "*", "")
        If KeyValue("close" + sCheck) = "false" Then Exit Function


        For Each p As Process In Process.GetProcesses
            If p.ProcessName Like sWildcard Then
                p.Kill()
            End If
        Next
    End Function


    Public Function GetProcess(ByVal sWildcard As String) As Process
        For Each p As Process In Process.GetProcesses
            If p.ProcessName Like sWildcard Then
                Return p
            End If
        Next
    End Function



    Public Function ExtractXML(sData As String, sStartKey As String, sEndKey As String)
        Dim iPos1 As Integer = InStr(1, sData, sStartKey)
        iPos1 = iPos1 + Len(sStartKey)
        Dim iPos2 As Integer = InStr(iPos1, sData, sEndKey)
        iPos2 = iPos2
        If iPos2 = 0 Then Return ""
        Dim sOut As String = Mid(sData, iPos1, iPos2 - iPos1)
        Return sOut
    End Function

End Module
