Imports System.Windows.Forms

Public Class frmConfiguration


    Private Sub InstallGridcoinGalazaToolStripMenuItem1_Click(sender As System.Object, e As System.EventArgs) Handles InstallGridcoinGalazaToolStripMenuItem1.Click
        InstallGalaza()
    End Sub

    Private Sub btnSave_Click(sender As System.Object, e As System.EventArgs) Handles btnSave.Click
        WriteSetting(chkSpeech, "enablespeech")
        MsgBox("Configuration Updated.", MsgBoxStyle.Information, "Configuration")

    End Sub
    Private Sub WriteSetting(oCheckBox As CheckBox, sSettingName As String)
        Dim sValue = IIf(oCheckBox.Checked, "true", "false")
        UpdateKey(sSettingName, sValue)
    End Sub

    Private Sub frmConfiguration_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        'Read keys
        lblTestnet.Text = IIf(mbTestNet, "TestNet", "")
        UpdateCheckbox(chkSpeech, "enablespeech")
    End Sub
    Private Sub UpdateCheckbox(oCheckbox As CheckBox, sSettingName As String)
        Dim sValue As String = KeyValue(sSettingName)
        oCheckbox.Checked = IIf(LCase(sValue) = "true", True, False)
    End Sub
End Class
