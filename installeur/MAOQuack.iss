; Installeur Windows de MAO Quack (Inno Setup 6). Fabriqué automatiquement par GitHub Actions.
; Installation pour l'utilisateur seulement : aucun mot de passe administrateur n'est demandé.

#ifndef Version
  #define Version "0.0.0"
#endif

[Setup]
AppId={{6B0C5B4E-51C7-4E0B-9B5D-3F2A7D0C9A41}
AppName=MAO Quack
AppVersion={#Version}
AppPublisher=Alpha Quack
AppPublisherURL=https://soundcloud.com/alpha-quack-773871380
DefaultDirName={autopf}\MAO Quack
DefaultGroupName=MAO Quack
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..
OutputBaseFilename=MAO-Quack-Installation
SetupIconFile=icone.ico
UninstallDisplayIcon={app}\MAO Quack.exe
UninstallDisplayName=MAO Quack
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "bureau"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"
Name: "association"; Description: "Ouvrir les morceaux (.quack) avec MAO Quack par un double-clic"; GroupDescription: "Fichiers :"

[Files]
Source: "..\sortie\MAO Quack\MAO Quack.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\MAO Quack"; Filename: "{app}\MAO Quack.exe"
Name: "{autodesktop}\MAO Quack"; Filename: "{app}\MAO Quack.exe"; Tasks: bureau

[Registry]
Root: HKA; Subkey: "Software\Classes\.quack"; ValueType: string; ValueName: ""; ValueData: "MAOQuack.Morceau"; Flags: uninsdeletevalue; Tasks: association
Root: HKA; Subkey: "Software\Classes\MAOQuack.Morceau"; ValueType: string; ValueName: ""; ValueData: "Morceau MAO Quack"; Flags: uninsdeletekey; Tasks: association
Root: HKA; Subkey: "Software\Classes\MAOQuack.Morceau\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\MAO Quack.exe,0"; Tasks: association
Root: HKA; Subkey: "Software\Classes\MAOQuack.Morceau\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\MAO Quack.exe"" ""%1"""; Tasks: association

[Run]
Filename: "{app}\MAO Quack.exe"; Description: "Lancer MAO Quack"; Flags: nowait postinstall skipifsilent
