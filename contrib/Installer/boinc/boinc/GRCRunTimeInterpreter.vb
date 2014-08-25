Option Explicit Off

Imports Microsoft.VisualBasic
Imports System
Imports System.Text
Imports System.CodeDom.Compiler
Imports System.Reflection
Imports System.IO
Imports System.Diagnostics



Module GRCRunTimeInterpreter

    Public Class cVBEvalProvider

        Private m_oCompilerErrors As CompilerErrorCollection

        Public Property CompilerErrors() As CompilerErrorCollection
            Get
                Return m_oCompilerErrors
            End Get
            Set(ByVal Value As CompilerErrorCollection)
                m_oCompilerErrors = Value
            End Set
        End Property

        Public Sub New()
            MyBase.New()
            m_oCompilerErrors = New CompilerErrorCollection
        End Sub

        Public Function Eval(ByVal vbCode As String) As Object

            Dim oCodeProvider As VBCodeProvider = New VBCodeProvider
     
            Dim oCParams As CompilerParameters = New CompilerParameters
            Dim oCResults As CompilerResults
            Dim oAssy As System.Reflection.Assembly
            Dim oExecInstance As Object = Nothing
            Dim oRetObj As Object = Nothing
            Dim oMethodInfo As MethodInfo
            Dim oType As Type
            Try

                ' Setup the Compiler Parameters - Add any referenced assemblies
                oCParams.ReferencedAssemblies.Add("system.dll")
                oCParams.ReferencedAssemblies.Add("system.xml.dll")
                oCParams.ReferencedAssemblies.Add("system.data.dll")
                oCParams.CompilerOptions = "/t:library"
                oCParams.GenerateInMemory = True
                ' Generate the Code Framework
                Dim sb As StringBuilder = New StringBuilder("")
                sb.AppendLine("Option Explicit Off")

                sb.Append("Imports System" & vbCrLf)
                sb.Append("Imports System.Xml" & vbCrLf)
                sb.Append("Imports System.Data" & vbCrLf)
                sb.Append("Imports Microsoft.VisualBasic" & vbCrLf)
                ' Build code, with our GRC passed in the middle 
                sb.Append("Namespace dValuate" & vbCrLf)
                sb.Append("Class EvalRunTime " & vbCrLf)
                sb.Append("Public Function EvaluateIt() As Object " & vbCrLf)
                sb.Append(vbCode & vbCrLf)
                sb.Append("End Function " & vbCrLf)
                sb.Append("End Class " & vbCrLf)
                sb.Append("End Namespace" & vbCrLf)
                Debug.WriteLine(sb.ToString())

                Try
                    ' Compile and get results 
                    ' 2.0 Framework - Method called from Code Provider
                    oCResults = oCodeProvider.CompileAssemblyFromSource(oCParams, sb.ToString)
                    ' 1.1 Framework - Method called from CodeCompiler Interface
                    ' Check for compile time errors 
                    If oCResults.Errors.Count <> 0 Then
                        Me.CompilerErrors = oCResults.Errors
                        Throw New Exception("Compile Errors")
                    Else
                        ' No Errors On Compile, so continue to process...
                        oAssy = oCResults.CompiledAssembly
                        oExecInstance = oAssy.CreateInstance("dValuate.EvalRunTime")
                        oType = oExecInstance.GetType
                        oMethodInfo = oType.GetMethod("EvaluateIt")
                        oRetObj = oMethodInfo.Invoke(oExecInstance, Nothing)
                        Return oRetObj
                    End If

                Catch ex As Exception
                    ' Compile Time Errors Are Caught Here
                    ' Some other weird error 
                    Debug.WriteLine(ex.Message)
                    Stop
                End Try

            Catch ex As Exception
                Debug.WriteLine(ex.Message)
                Stop
            End Try

            Return oRetObj

        End Function

    End Class


    Private Function SampleRoutine() As StringBuilder
        Dim oRetSB As New StringBuilder
        Dim sText As String = "July 30, 2006"
        oRetSB.Append("If Date.Parse(""" & sText & """) < System.DateTime.Now() Then " & vbCrLf)
        oRetSB.Append(" Return ""OLD"" " & vbCrLf)
        oRetSB.Append("Else " & vbCrLf)
        oRetSB.Append(" Return ""NEW"" " & vbCrLf)
        oRetSB.Append("End If " & vbCrLf)
        oRetSB = New StringBuilder
        oRetSB.AppendLine("MsgBox(" + Chr(34) + "Hello!" + Chr(34) + ",MsgBoxStyle.Critical," + Chr(34) + "Message Title" + Chr(34) + ")")
        Return oRetSB
    End Function

    Public Function CompileAndRunCode(ByVal VBCodeToExecute As String) As Object

        Dim sReturn_DataType As String
        Dim sReturn_Value As String = ""
        Try

            Dim ep As New cVBEvalProvider
            ' Compile and run
            Dim objResult As Object = ep.Eval(VBCodeToExecute)
            If ep.CompilerErrors.Count <> 0 Then
                Diagnostics.Debug.WriteLine("CompileAndRunCode: Compile Error Count = " & ep.CompilerErrors.Count)
                Diagnostics.Debug.WriteLine(ep.CompilerErrors.Item(0))
                Return "ERROR" ' Forget it
            End If
            Dim t As Type = objResult.GetType()
            If t.ToString() = "System.String" Then
                sReturn_DataType = t.ToString
                sReturn_Value = Convert.ToString(objResult)
            Else
                ' Some other type of data 
            End If

        Catch ex As Exception
            Dim sErrMsg As String
            sErrMsg = String.Format("{0}", ex.Message)
        End Try

        Return sReturn_Value

    End Function
End Module
