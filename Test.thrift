namespace cpp Artifact

struct Example {
  1: i32 id,
  2: string name
}

service TestService {
  void ping(),
  i32 add(1:i32 a, 2:i32 b)
}