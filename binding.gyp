{
  "targets": [
    {
      "target_name": "asteroidstest",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
        "cpp/src/AsteroidGenerator.cpp",
        "cpp/src/Asteroid.cpp",
        "cpp/src/Point2D.cpp"
      ],
      "include_dirs": [
        '<!@(node -p "require(\'node-addon-api\').include")',
        "cpp/include"
      ],
      "defines": [ 
        'NAPI_DISABLE_CPP_EXCEPTIONS',
        'ASTEROIDS_TEST'
       ]
    },
    {
      "target_name": "collidertest",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
        "cpp/src/AsteroidGenerator.cpp",
        "cpp/src/Asteroid.cpp",
        "cpp/src/AsteroidCollider.cpp",
        "cpp/src/Point2D.cpp"
      ],
      "include_dirs": [
        '<!@(node -p "require(\'node-addon-api\').include")',
        "cpp/include"
      ],
      "defines": [ 
        'NAPI_DISABLE_CPP_EXCEPTIONS',
        'COLLIDER_TEST'
      ]
    }
  ]
}